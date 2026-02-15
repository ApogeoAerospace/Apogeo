#define _USE_MATH_DEFINES
#include <cmath>
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

    // Apply integrator type from physics config
    const auto& physics_config = config.getPhysicsConfig();
    if (physics_integrator_) {
        physics_integrator_->setIntegratorType(physics_config.integrator_type);
        LOG_INFO("Integrator type set to: " + physics_config.integrator_type, "PluginManager");
    }

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

void PluginManager::run_simulation_cycle(std::vector<uint8_t>& state_buffer) {
    run_simulation_cycle_improved(state_buffer, 0.01); // Default time step
}

void PluginManager::run_simulation_cycle_improved(std::vector<uint8_t>& state_buffer, double delta_time) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(plugins_mutex_);

    // Crear datos del tick
    PluginTickData tick_data = {};
    tick_data.state_buffer = state_buffer.data();
    tick_data.buffer_size = state_buffer.size();
    tick_data.delta_time = delta_time;

    // Fase 1: Ejecutar plugins secuenciales
    execute_sequential_plugins(state_buffer);

    // Fase 2: Ejecutar plugins paralelos
    execute_parallel_plugins(state_buffer);

    // Fase 3: Aplicar integración física
    apply_physics_integration(state_buffer, delta_time);

    // Actualizar métricas
    total_cycles_++;
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    total_cycle_time_.store(total_cycle_time_.load() + duration.count() / 1000.0); // Convert to milliseconds
}

void PluginManager::execute_sequential_plugins(std::vector<uint8_t>& state_buffer) {
    PluginTickData tick_data = {};
    tick_data.state_buffer = state_buffer.data();
    tick_data.buffer_size = state_buffer.size();
    tick_data.output_force = nullptr;
    tick_data.output_torque = nullptr;

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

void PluginManager::execute_parallel_plugins(std::vector<uint8_t>& state_buffer) {
    std::vector<std::future<void>> futures;
    std::vector<PluginVector3> forces;
    std::vector<PluginVector3> torques;
    std::vector<double> masses;

    // Get parallel plugins
    auto parallel_plugins = get_plugins_by_type(PluginType::PARALLEL_PHYSICS_CALCULATOR);

    if (parallel_plugins.empty()) {
        // Reset forces if no plugins
        std::lock_guard<std::mutex> lock(force_mutex_);
        accumulated_force_ = {0.0f, 0.0f, 0.0f};
        accumulated_torque_ = {0.0f, 0.0f, 0.0f};
        accumulated_mass_ = 0.0;
        return;
    }

    forces.resize(parallel_plugins.size());
    torques.resize(parallel_plugins.size());
    masses.resize(parallel_plugins.size(), 0.0);

    // Execute plugins in parallel
    for (size_t i = 0; i < parallel_plugins.size(); ++i) {
        auto& plugin = *parallel_plugins[i];

        futures.emplace_back(std::async(std::launch::async, [&plugin, &state_buffer, &forces, &torques, &masses, i]() {
            auto start_time = std::chrono::high_resolution_clock::now();

            PluginTickData tick_data = {};
            tick_data.state_buffer = state_buffer.data();
            tick_data.buffer_size = state_buffer.size();
            tick_data.output_force = &forces[i];
            tick_data.output_torque = &torques[i];
            tick_data.output_mass = &masses[i];

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
    double reported_mass = 0.0;

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

    // Use last non-zero mass reported by any plugin
    for (const auto& m : masses) {
        if (m > 0.0) reported_mass = m;
    }

    // Store total forces for physics integration
    {
        std::lock_guard<std::mutex> lock(force_mutex_);
        accumulated_force_ = total_force;
        accumulated_torque_ = total_torque;
        accumulated_mass_ = reported_mass;
    }

    LOG_DEBUG("Total forces calculated: F=(" + std::to_string(total_force.x) + ", " +
              std::to_string(total_force.y) + ", " + std::to_string(total_force.z) +
              ") T=(" + std::to_string(total_torque.x) + ", " +
              std::to_string(total_torque.y) + ", " + std::to_string(total_torque.z) +
              ") Mass=" + std::to_string(reported_mass), "PluginManager");
}

void PluginManager::apply_physics_integration(std::vector<uint8_t>& state_buffer, double delta_time) {
    if (!physics_integrator_) {
        return;
    }

    // Parse current state from FlatBuffer
    const state_vector::GeneralState* current_state = state_vector::GetGeneralState(state_buffer.data());
    if (!current_state) {
        LOG_ERROR("Failed to parse state buffer in physics integration", "PluginManager");
        return;
    }

    PhysicsState physics_state = MoLab::PhysicsIntegrator::fromFlatBuffer(current_state);

    // Get physics configuration
    const auto& physics_config = ConfigManager::getInstance().getPhysicsConfig();

    // --- Coordinate system: convert geocentric Z to altitude above sea level ---
    const double EARTH_RADIUS = 6378137.0; // m (WGS84)
    double pos_z = physics_state.position.z;
    double distance_from_center = std::sqrt(
        physics_state.position.x * physics_state.position.x +
        physics_state.position.y * physics_state.position.y +
        pos_z * pos_z);
    bool is_geocentric = (distance_from_center > 1000000.0 || std::abs(pos_z) > 1000000.0);
    double altitude_asl = is_geocentric ? (distance_from_center - EARTH_RADIUS) : pos_z;
    if (altitude_asl < 0) altitude_asl = 0;

    // --- Mass: use plugin-reported mass if available, else config vehicle_mass ---
    PluginVector3 plugin_force;
    PluginVector3 plugin_torque;
    double plugin_mass;
    {
        std::lock_guard<std::mutex> lock(force_mutex_);
        plugin_force = accumulated_force_;
        plugin_torque = accumulated_torque_;
        plugin_mass = accumulated_mass_;
    }

    double current_mass = (plugin_mass > 0.0) ? plugin_mass : physics_config.vehicle_mass;
    if (current_mass < 1.0) current_mass = 1.0; // safety floor
    physics_state.mass = current_mass;

    // Convert plugin forces to Vector3 (includes thrust from propulsion plugin)
    Vector3 total_force(plugin_force.x, plugin_force.y, plugin_force.z);
    Vector3 total_torque(plugin_torque.x, plugin_torque.y, plugin_torque.z);

    // --- GRAVITY (core fundamental physics) ---
    if (physics_config.enable_gravity) {
        double g = physics_config.gravity_magnitude;
        total_force = total_force + Vector3(0.0, 0.0, -g * current_mass);
    }

    // --- ATMOSPHERIC DRAG (core fundamental physics, altitude ASL) ---
    if (physics_config.enable_atmospheric_drag) {
        double air_density = AtmosphericEffects::calculateAirDensity(altitude_asl);
        Vector3 drag = AtmosphericEffects::calculateDrag(
            physics_state.velocity, air_density,
            physics_config.drag_coefficient, physics_config.reference_area);
        total_force = total_force + drag;
    }

    // Integrate physics using configured integrator type
    PhysicsState new_state = physics_integrator_->integrate(physics_state, total_force, total_torque, delta_time);

    // --- Atmospheric conditions for new position (using altitude ASL) ---
    double new_distance = std::sqrt(
        new_state.position.x * new_state.position.x +
        new_state.position.y * new_state.position.y +
        new_state.position.z * new_state.position.z);
    double new_alt_asl = is_geocentric ? (new_distance - EARTH_RADIUS) : new_state.position.z;
    if (new_alt_asl < 0) new_alt_asl = 0;

    float new_atm_density = static_cast<float>(AtmosphericEffects::calculateAirDensity(new_alt_asl));

    // ISA temperature model
    float new_atm_temperature = static_cast<float>(
        (new_alt_asl <= 11000.0) ? 288.15 - 0.0065 * new_alt_asl : 216.65);

    // ISA pressure model
    float new_atm_pressure;
    if (new_alt_asl <= 11000.0) {
        double T0 = 288.15, P0 = 101325.0, L = 0.0065, R = 287.05, g_std = 9.80665;
        double T = T0 - L * new_alt_asl;
        new_atm_pressure = static_cast<float>(P0 * pow(T / T0, g_std / (R * L)));
    } else {
        double P11 = 22632.1, T11 = 216.65, R = 287.05, g_std = 9.80665;
        new_atm_pressure = static_cast<float>(P11 * exp(-g_std * (new_alt_asl - 11000.0) / (R * T11)));
    }

    // Create new FlatBuffer with updated values
    flatbuffers::FlatBufferBuilder builder;

    auto position = state_vector::Vec3(new_state.position.x, new_state.position.y, new_state.position.z);
    auto velocity = state_vector::Vec3(new_state.velocity.x, new_state.velocity.y, new_state.velocity.z);
    auto orientation = state_vector::Quaternion(new_state.orientation.x, new_state.orientation.y, new_state.orientation.z, 1.0f);

    float gravity_z = physics_config.enable_gravity ?
        static_cast<float>(-physics_config.gravity_magnitude) : 0.0f;
    auto gravity = state_vector::Vec3(0.0f, 0.0f, gravity_z);

    auto wind_speed = state_vector::Vec3(
        current_state->wind_speed() ? current_state->wind_speed()->x() : 0.0f,
        current_state->wind_speed() ? current_state->wind_speed()->y() : 0.0f,
        current_state->wind_speed() ? current_state->wind_speed()->z() : 0.0f
    );

    auto general_state = state_vector::CreateGeneralState(builder,
        &position, &velocity, &orientation,
        new_atm_density, new_atm_pressure, new_atm_temperature,
        &gravity, static_cast<float>(new_state.time), current_state->UTC(), &wind_speed
    );

    builder.Finish(general_state);

    // Update state buffer
    const uint8_t* new_buffer = builder.GetBufferPointer();
    uint32_t new_size = builder.GetSize();
    state_buffer.assign(new_buffer, new_buffer + new_size);

    LOG_DEBUG("Physics integrated. Time: " + std::to_string(new_state.time) +
              ", Alt ASL: " + std::to_string(new_alt_asl) +
              ", Mass: " + std::to_string(current_mass) +
              ", Plugin F=(" + std::to_string(plugin_force.x) + "," + std::to_string(plugin_force.y) + "," + std::to_string(plugin_force.z) +
              "), Atm: [rho=" + std::to_string(new_atm_density) +
              ", P=" + std::to_string(new_atm_pressure) +
              ", T=" + std::to_string(new_atm_temperature) + "]", "PluginManager");
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
