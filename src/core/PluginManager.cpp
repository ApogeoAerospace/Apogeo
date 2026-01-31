#include "PluginManager.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "PhysicsIntegrator.h"
#include "state_vector_generated.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <chrono>
#include <thread>
#include <numeric>
#include <mutex>
#include <future>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace MoLab {

PluginManager::PluginManager() : physics_integrator_(std::make_unique<PhysicsIntegrator>()) {
    LOG_INFO("PluginManager initialized", "PluginManager");
}

PluginManager::~PluginManager() {
    shutdown();
}

bool PluginManager::load_plugin(const std::string& path, PluginType type) {
    // Mutex ya obtenido por load_plugins_from_config() - no necesitamos otro lock aquí

    LoadedPlugin plugin;
    plugin.type = type;
    plugin.path = path;
    plugin.handle = nullptr;

    LOG_INFO("Loading plugin: " + path, "PluginManager");

    // Carga de biblioteca compartida según el sistema operativo
#ifdef _WIN32
    plugin.lib_handle = LoadLibrary(path.c_str());
    if (!plugin.lib_handle) {
        LOG_ERROR("Failed to load plugin library: " + path, "PluginManager");
        return false;
    }
    // Obtiene las direcciones de las funciones de la API del plugin
    plugin.create_func = reinterpret_cast<decltype(plugin.create_func)>(GetProcAddress(plugin.lib_handle, "plugin_create_instance"));
    plugin.configure_func = reinterpret_cast<decltype(plugin.configure_func)>(GetProcAddress(plugin.lib_handle, "plugin_configure"));
    plugin.tick_func = reinterpret_cast<decltype(plugin.tick_func)>(GetProcAddress(plugin.lib_handle, "plugin_tick"));
    plugin.destroy_func = reinterpret_cast<decltype(plugin.destroy_func)>(GetProcAddress(plugin.lib_handle, "plugin_destroy_instance"));
#else // POSIX
    plugin.lib_handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!plugin.lib_handle) {
        LOG_ERROR("Failed to load plugin library: " + path + " - " + std::string(dlerror()), "PluginManager");
        return false;
    }
    // Obtiene las direcciones de las funciones de la API del plugin
    plugin.create_func = reinterpret_cast<decltype(plugin.create_func)>(dlsym(plugin.lib_handle, "plugin_create_instance"));
    plugin.configure_func = reinterpret_cast<decltype(plugin.configure_func)>(dlsym(plugin.lib_handle, "plugin_configure"));
    plugin.tick_func = reinterpret_cast<decltype(plugin.tick_func)>(dlsym(plugin.lib_handle, "plugin_tick"));
    plugin.destroy_func = reinterpret_cast<decltype(plugin.destroy_func)>(dlsym(plugin.lib_handle, "plugin_destroy_instance"));
#endif

    if (plugin.create_func == nullptr || plugin.tick_func == nullptr || plugin.destroy_func == nullptr) {
        LOG_ERROR("Failed to find required plugin API functions in: " + path, "PluginManager");
        // Note: configure_func is optional for backward compatibility
#ifdef _WIN32
        FreeLibrary(plugin.lib_handle);
#else
        dlclose(plugin.lib_handle);
#endif
        return false;
    }

    // Crear instancia del plugin
    plugin.handle = plugin.create_func();
    if (plugin.handle == nullptr) {
        LOG_ERROR("Failed to create plugin instance: " + path, "PluginManager");
#ifdef _WIN32
        FreeLibrary(plugin.lib_handle);
#else
        dlclose(plugin.lib_handle);
#endif
        return false;
    }

    loaded_plugins_.emplace_back(std::move(plugin));
    LOG_INFO("Plugin loaded successfully: " + path, "PluginManager");
    return true;
}

bool PluginManager::load_plugins_from_config() {
    std::lock_guard<std::mutex> lock(plugins_mutex_);

    const auto& config = ConfigManager::getInstance();
    const auto& plugin_configs = config.getPluginConfigs();

    LOG_INFO("Loading " + std::to_string(plugin_configs.size()) + " plugins from configuration", "PluginManager");

    bool all_loaded = true;
    for (const auto& plugin_config : plugin_configs) {
        if (!plugin_config.enabled) {
            LOG_INFO("Skipping disabled plugin: " + plugin_config.name, "PluginManager");
            continue;
        }

        PluginType type = static_cast<PluginType>(plugin_config.type);
        if (!load_plugin(plugin_config.library_path, type)) {
            LOG_ERROR("Failed to load plugin from config: " + plugin_config.name, "PluginManager");
            all_loaded = false;
            continue;
        }

        if (!loaded_plugins_.empty()) {
            loaded_plugins_.back().name = plugin_config.name;
        }

        // Configure plugin with parameters if available
        if (!plugin_config.parameters.empty() && !loaded_plugins_.empty()) {
            auto& last_plugin = loaded_plugins_.back();
            if (last_plugin.configure_func) {
                std::string params_str = plugin_config.parameters.dump();
                int32_t config_result = last_plugin.configure_func(last_plugin.handle, params_str.c_str());
                if (config_result != 0) {
                    LOG_ERROR("Failed to configure plugin " + plugin_config.name + " with parameters. Error code: " + std::to_string(config_result), "PluginManager");
                    all_loaded = false;
                } else {
                    LOG_INFO("Plugin " + plugin_config.name + " configured successfully with parameters", "PluginManager");
                }
            } else {
                LOG_WARNING("Plugin " + plugin_config.name + " does not support configuration (missing plugin_configure function)", "PluginManager");
            }
        }
    }

    return all_loaded;
}

void PluginManager::run_simulation_cycle(std::vector<uint8_t>& state_buffer, double delta_time) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(plugins_mutex_);

    // Fase 1: Ejecutar plugins secuenciales
    execute_sequential_plugins(state_buffer, delta_time);

    // Fase 2: Ejecutar plugins paralelos
    execute_parallel_plugins(state_buffer, delta_time);

    // Fase 3: Aplicar integración física
    apply_physics_integration(state_buffer, delta_time);

    // Actualizar métricas
    total_cycles_++;
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    total_cycle_time_.store(total_cycle_time_.load() + duration.count() / 1000.0); // Convert to milliseconds
}

void PluginManager::execute_sequential_plugins(std::vector<uint8_t>& state_buffer, double delta_time) {
    PluginTickData tick_data = {};
    tick_data.state_buffer = state_buffer.data();
    tick_data.buffer_size = state_buffer.size();
    tick_data.delta_time = delta_time;
    tick_data.output_force = nullptr;
    tick_data.output_torque = nullptr;
    tick_data.force_out = nullptr;
    tick_data.torque_out = nullptr;

    for (auto& plugin : loaded_plugins_) {
        if (!plugin.enabled || plugin.type != PluginType::SEQUENTIAL_STATE_MODIFIER) {
            continue;
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        int32_t result = plugin.tick_func(plugin.handle, &tick_data);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double exec_time = duration.count() / 1000.0; // Convert to milliseconds

        // Update metrics
        plugin.execution_count++;
        plugin.last_execution_time.store(exec_time);
        plugin.total_execution_time.store(plugin.total_execution_time.load() + exec_time);

        if (result != 0) {
            LOG_WARNING("Sequential plugin returned error code: " + std::to_string(result), "PluginManager");
        }
    }
}

void PluginManager::execute_parallel_plugins(std::vector<uint8_t>& state_buffer, double delta_time) {
    std::vector<std::future<void>> futures;
    std::vector<PluginVector3> forces;
    std::vector<PluginVector3> torques;

    // Get parallel plugins
    auto parallel_plugins = get_plugins_by_type(PluginType::PARALLEL_PHYSICS_CALCULATOR);

    if (parallel_plugins.empty()) {
        // Reset forces if no plugins
        std::lock_guard<std::mutex> lock(force_mutex_);
        accumulated_force_ = {0.0f, 0.0f, 0.0f};
        accumulated_torque_ = {0.0f, 0.0f, 0.0f};
        return;
    }

    forces.resize(parallel_plugins.size());
    torques.resize(parallel_plugins.size());

    // Execute plugins in parallel
    for (size_t i = 0; i < parallel_plugins.size(); ++i) {
        auto& plugin = *parallel_plugins[i];

        futures.emplace_back(std::async(std::launch::async, [&plugin, &state_buffer, &forces, &torques, delta_time, i]() {
            auto start_time = std::chrono::high_resolution_clock::now();

            PluginTickData tick_data = {};
            tick_data.state_buffer = state_buffer.data();
            tick_data.buffer_size = state_buffer.size();
            tick_data.delta_time = delta_time;
            tick_data.output_force = &forces[i];
            tick_data.output_torque = &torques[i];
            tick_data.force_out = &forces[i];
            tick_data.torque_out = &torques[i];

            int32_t result = plugin.tick_func(plugin.handle, &tick_data);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            double exec_time = duration.count() / 1000.0;

            // Update metrics
            plugin.execution_count++;
            plugin.last_execution_time.store(exec_time);
            plugin.total_execution_time.store(plugin.total_execution_time.load() + exec_time);

            if (result != 0) {
                LOG_WARNING("Parallel plugin returned error code: " + std::to_string(result), "PluginManager");
            }
        }));
    }

    // Wait for all plugins to complete
    for (auto& future : futures) {
        future.wait();
    }

    // Sum up forces and torques
    PluginVector3 total_force = {0.0f, 0.0f, 0.0f};
    PluginVector3 total_torque = {0.0f, 0.0f, 0.0f};

    for (const auto& force : forces) {
        total_force.x += force.x;
        total_force.y += force.y;
        total_force.z += force.z;
    }

    for (const auto& torque : torques) {
        total_torque.x += torque.x;
        total_torque.y += torque.y;
        total_torque.z += torque.z;
    }

    // Store total forces for physics integration
    {
        std::lock_guard<std::mutex> lock(force_mutex_);
        accumulated_force_ = total_force;
        accumulated_torque_ = total_torque;
    }

    LOG_DEBUG("Total forces calculated: F=(" + std::to_string(total_force.x) + ", " +
              std::to_string(total_force.y) + ", " + std::to_string(total_force.z) +
              ") T=(" + std::to_string(total_torque.x) + ", " +
              std::to_string(total_torque.y) + ", " + std::to_string(total_torque.z) + ")", "PluginManager");
}

void PluginManager::apply_physics_integration(std::vector<uint8_t>& state_buffer, double delta_time) {
    if (!physics_integrator_) {
        return;
    }

    // Leer estado actual del FlatBuffer
    const state_vector::GeneralState* current_state = state_vector::GetGeneralState(state_buffer.data());
    if (!current_state) {
        LOG_ERROR("Failed to parse state buffer in physics integration", "PluginManager");
        return;
    }

    // Convertir a estado interno para integración
    PhysicsState physics_state = MoLab::PhysicsIntegrator::fromFlatBuffer(current_state);

    // Recuperar fuerzas/torques acumulados de plugins paralelos
    PluginVector3 plugin_force;
    PluginVector3 plugin_torque;
    {
        std::lock_guard<std::mutex> lock(force_mutex_);
        plugin_force = accumulated_force_;
        plugin_torque = accumulated_torque_;
    }

    Vector3 total_force(plugin_force.x, plugin_force.y, plugin_force.z);
    Vector3 total_torque(plugin_torque.x, plugin_torque.y, plugin_torque.z);

    // Integrar física (PhysicsIntegrator actualiza sim_time internamente)
    PhysicsState new_state = physics_integrator_->integrate(physics_state, total_force, total_torque, delta_time);

    // Reconstruir el FlatBuffer con el esquema nuevo, preservando campos
    flatbuffers::FlatBufferBuilder builder;

    // Cinemática actualizada
    auto position = state_vector::Vec3(new_state.position.x, new_state.position.y, new_state.position.z);
    auto velocity = state_vector::Vec3(new_state.velocity.x, new_state.velocity.y, new_state.velocity.z);
    auto orientation = state_vector::Quaternion(new_state.orientation.x, new_state.orientation.y, new_state.orientation.z, 1.0f);
    auto angular_velocity = state_vector::Vec3(new_state.angular_velocity.x, new_state.angular_velocity.y, new_state.angular_velocity.z);

    // Entorno: usar punteros del estado actual (si existen)
    const state_vector::Vec3* gravity_ptr = current_state->gravity();
    const state_vector::Vec3* wind_ptr = current_state->wind_velocity();

    // Propiedades de masa y CG
    float total_mass = current_state->total_mass();
    state_vector::Vec3 cg_loc = current_state->cg_location()
        ? state_vector::Vec3(current_state->cg_location()->x(), current_state->cg_location()->y(), current_state->cg_location()->z())
        : state_vector::Vec3(0.0f, 0.0f, 0.0f);

    state_vector::InertiaTensor inertia_tensor(
        current_state->inertia_tensor() ? current_state->inertia_tensor()->ixx() : 0.0f,
        current_state->inertia_tensor() ? current_state->inertia_tensor()->iyy() : 0.0f,
        current_state->inertia_tensor() ? current_state->inertia_tensor()->izz() : 0.0f,
        current_state->inertia_tensor() ? current_state->inertia_tensor()->ixy() : 0.0f,
        current_state->inertia_tensor() ? current_state->inertia_tensor()->ixz() : 0.0f,
        current_state->inertia_tensor() ? current_state->inertia_tensor()->iyz() : 0.0f
    );

    // Air data
    float mach_number = current_state->mach_number();
    float dynamic_pressure = current_state->dynamic_pressure();
    float angle_of_attack = current_state->angle_of_attack();
    float sideslip_angle = current_state->sideslip_angle();

    float atm_density = current_state->atm_density();
    float atm_pressure = current_state->atm_pressure();
    float atm_temperature = current_state->atm_temperature();

    // Arrays: copiar si existen
    flatbuffers::Offset<flatbuffers::Vector<float>> propellant_masses_fb;
    if (auto pm = current_state->propellant_masses()) {
        std::vector<float> pm_vec;
        pm_vec.reserve(pm->size());
        for (auto v : *pm) pm_vec.push_back(v);
        propellant_masses_fb = builder.CreateVector(pm_vec);
    }

    // Arrays: engines (vector of structs) - preserve by copying
    flatbuffers::Offset<flatbuffers::Vector<const state_vector::EngineCmd*>> engines_fb;
    if (auto engines = current_state->engines()) {
        std::vector<state_vector::EngineCmd> eng_vec;
        eng_vec.reserve(engines->size());
        for (size_t i = 0; i < engines->size(); ++i) {
            const state_vector::EngineCmd* ec = engines->Get(i);
            // tvc_angles is a struct; access via '.' on the returned struct
            const state_vector::Vec3 tvc = ec->tvc_angles();
            eng_vec.emplace_back(ec->throttle(), tvc);
        }
        engines_fb = builder.CreateVectorOfStructs(eng_vec);
    }

    flatbuffers::Offset<flatbuffers::Vector<float>> surface_deflections_fb;
    if (auto sd = current_state->surface_deflections()) {
        std::vector<float> sd_vec;
        sd_vec.reserve(sd->size());
        for (auto v : *sd) sd_vec.push_back(v);
        surface_deflections_fb = builder.CreateVector(sd_vec);
    }

    // Construir GeneralState preservando dt (o actualizándolo con delta_time)
    state_vector::GeneralStateBuilder gs_builder(builder);
    gs_builder.add_sim_time(static_cast<float>(new_state.time));
    // Preserve the integrator step in the buffer to avoid dt becoming 0 after rebuild.
    // Option A: keep previous dt from buffer
    gs_builder.add_dt(current_state->dt());
    // Option B (if you want dt to reflect the actual integrator step used):
    // gs_builder.add_dt(static_cast<float>(delta_time));

    gs_builder.add_position(&position);
    gs_builder.add_velocity(&velocity);
    gs_builder.add_orientation(&orientation);
    gs_builder.add_angular_velocity(&angular_velocity);

    gs_builder.add_total_mass(total_mass);
    gs_builder.add_cg_location(&cg_loc);
    gs_builder.add_inertia_tensor(&inertia_tensor);
    if (propellant_masses_fb.o != 0) gs_builder.add_propellant_masses(propellant_masses_fb);

    gs_builder.add_mach_number(mach_number);
    gs_builder.add_dynamic_pressure(dynamic_pressure);
    gs_builder.add_angle_of_attack(angle_of_attack);
    gs_builder.add_sideslip_angle(sideslip_angle);

    gs_builder.add_atm_density(atm_density);
    gs_builder.add_atm_pressure(atm_pressure);
    gs_builder.add_atm_temperature(atm_temperature);

    if (wind_ptr) gs_builder.add_wind_velocity(wind_ptr);
    if (gravity_ptr) gs_builder.add_gravity(gravity_ptr);

    if (engines_fb.o != 0) gs_builder.add_engines(engines_fb);
    if (surface_deflections_fb.o != 0) gs_builder.add_surface_deflections(surface_deflections_fb);

    auto general_state = gs_builder.Finish();
    builder.Finish(general_state);

    // Actualizar buffer de estado
    const uint8_t* new_buffer = builder.GetBufferPointer();
    uint32_t new_size = builder.GetSize();
    state_buffer.assign(new_buffer, new_buffer + new_size);

    LOG_DEBUG(
        "Physics integrated; sim_time=" + std::to_string(new_state.time) +
        " pos=(" + std::to_string(new_state.position.x) + ", " +
        std::to_string(new_state.position.y) + ", " + std::to_string(new_state.position.z) + ")",
        "PluginManager"
    );
}

std::vector<LoadedPlugin*> PluginManager::get_plugins_by_type(PluginType type) {
    std::vector<LoadedPlugin*> result;
    for (auto& plugin : loaded_plugins_) {
        if (plugin.type == type && plugin.enabled) {
            result.push_back(&plugin);
        }
    }
    return result;
}

size_t PluginManager::get_plugin_count() const {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    return loaded_plugins_.size();
}

std::vector<std::string> PluginManager::get_loaded_plugin_names() const {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    std::vector<std::string> names;
    for (const auto& plugin : loaded_plugins_) {
        names.push_back(plugin.name.empty() ? plugin.path : plugin.name);
    }
    return names;
}

std::vector<PluginManager::PluginMetrics> PluginManager::get_plugin_metrics() const {
    std::lock_guard<std::mutex> lock(plugins_mutex_);
    std::vector<PluginMetrics> metrics;

    for (const auto& plugin : loaded_plugins_) {
        PluginMetrics metric;
        metric.name = plugin.name.empty() ? plugin.path : plugin.name;
        metric.execution_count = plugin.execution_count.load();
        metric.total_execution_time = plugin.total_execution_time.load();
        metric.last_execution_time = plugin.last_execution_time.load();
        metric.average_execution_time = metric.execution_count > 0 ?
            metric.total_execution_time / metric.execution_count : 0.0;

        metrics.push_back(metric);
    }

    return metrics;
}

void PluginManager::shutdown() {
    std::lock_guard<std::mutex> lock(plugins_mutex_);

    LOG_INFO("Shutting down PluginManager", "PluginManager");

    for (auto& plugin : loaded_plugins_) {
        cleanup_plugin(plugin);
    }

    loaded_plugins_.clear();
    physics_integrator_.reset();

    LOG_INFO("PluginManager shutdown complete", "PluginManager");
}

void PluginManager::cleanup_plugin(LoadedPlugin& plugin) {
    if (plugin.handle != nullptr && plugin.destroy_func != nullptr) {
        plugin.destroy_func(plugin.handle);
        plugin.handle = nullptr;
    }

    if (plugin.lib_handle) {
#ifdef _WIN32
        FreeLibrary(plugin.lib_handle);
#else
        dlclose(plugin.lib_handle);
#endif
        plugin.lib_handle = nullptr;
    }
}

} // namespace MoLab
