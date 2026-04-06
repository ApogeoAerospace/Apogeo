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
#include <condition_variable>
#include <deque>
#include <functional>
#include <filesystem>

/**
 * @file PluginManager.cpp
 * @brief Implementation of plugin load and execution manager.
 */

#ifdef _WIN32
#include <windows.h>
#endif

namespace MoLab {

namespace {

/**
 * @brief Logging adapter so plugins can use the host logger.
 */
void plugin_host_log_bridge(int32_t level, const char* component, const char* message, void* user_data) {
    (void)user_data;

    const std::string component_name = component ? component : "Plugin";
    const std::string log_message = message ? message : "";

    switch (level) {
        case PLUGIN_LOG_DEBUG:
            LOG_DEBUG(log_message, component_name);
            break;
        case PLUGIN_LOG_INFO:
            LOG_INFO(log_message, component_name);
            break;
        case PLUGIN_LOG_WARNING:
            LOG_WARNING(log_message, component_name);
            break;
        case PLUGIN_LOG_ERROR:
            LOG_ERROR(log_message, component_name);
            break;
        case PLUGIN_LOG_CRITICAL:
            LOG_CRITICAL(log_message, component_name);
            break;
        default:
            LOG_INFO(log_message, component_name);
            break;
    }
}

/**
 * @brief Returns a stable host-services instance for plugins.
 */
const PluginHostServices* get_stable_host_services() {
    static PluginHostServices services = [] {
        PluginHostServices s = {};
        s.api_version = 1;
        s.log = &plugin_host_log_bridge;
        s.user_data = nullptr;
        return s;
    }();

    return &services;
}

} // namespace

static std::string normalize_plugin_path(const std::string& path) {
    if (path.find(".dll") != std::string::npos ||
        path.find(".dylib") != std::string::npos ||
        path.find(".so") != std::string::npos) {
        return path;
    }

    // Split into directory and basename
    std::string dir;
    std::string basename;
    auto sep = path.rfind('/');
#ifdef _WIN32
    auto sep2 = path.rfind('\\');
    if (sep2 != std::string::npos && (sep == std::string::npos || sep2 > sep)) {
        sep = sep2;
    }
#endif
    if (sep != std::string::npos) {
        dir = path.substr(0, sep + 1);
        basename = path.substr(sep + 1);
    } else {
        dir = "";
        basename = path;
    }

#ifdef _WIN32
    std::filesystem::path normalized(dir + basename + ".dll");
    return normalized.lexically_normal().generic_string();
#elif __APPLE__
    if (basename.substr(0, 3) != "lib") {
        basename = "lib" + basename;
    }
    std::filesystem::path normalized(dir + basename + ".dylib");
    return normalized.lexically_normal().generic_string();
#else
    if (basename.substr(0, 3) != "lib") {
        basename = "lib" + basename;
    }
    std::filesystem::path normalized(dir + basename + ".so");
    return normalized.lexically_normal().generic_string();
#endif
}

static std::vector<std::string> build_plugin_candidates(const std::string& normalized_path) {
    namespace fs = std::filesystem;

    std::vector<std::string> candidates;
    auto append_unique = [&candidates](const fs::path& candidate) {
        std::string normalized = candidate.lexically_normal().generic_string();
        if (normalized.empty()) {
            return;
        }

        if (std::find(candidates.begin(), candidates.end(), normalized) == candidates.end()) {
            candidates.push_back(normalized);
        }
    };

    fs::path plugin_path(normalized_path);
    append_unique(plugin_path);

    if (plugin_path.is_relative()) {
        std::error_code ec;
        const fs::path cwd = fs::current_path(ec);
        if (!ec) {
            append_unique(cwd / plugin_path);

            const fs::path filename = plugin_path.filename();
            if (!filename.empty()) {
                append_unique(cwd / filename);
                append_unique(cwd / "lib" / filename);
                append_unique(cwd.parent_path() / "lib" / filename);
                append_unique(cwd.parent_path() / "bin" / filename);
            }
        }
    }

    return candidates;
}

// Simple scheduler to run plugin tasks in parallel.
class PluginTaskScheduler {
public:
    explicit PluginTaskScheduler(size_t thread_count) {
        size_t count = std::max<size_t>(1, thread_count);
        for (size_t i = 0; i < count; ++i) {
            workers_.emplace_back([this]() { worker_loop(); });
        }
    }

    ~PluginTaskScheduler() {
        stop();
    }

    std::future<void> submit(std::function<void()> task) {
        Task queued;
        queued.func = std::move(task);
        std::future<void> future = queued.promise.get_future();
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            tasks_.emplace_back(std::move(queued));
        }
        queue_cv_.notify_one();
        return future;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            stopping_ = true;
        }
        queue_cv_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
    }

private:
    struct Task {
        std::function<void()> func;
        std::promise<void> promise;
    };

    void worker_loop() {
        for (;;) {
            Task task;
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this]() { return stopping_ || !tasks_.empty(); });
                if (stopping_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop_front();
            }
            try {
                task.func();
                task.promise.set_value();
            } catch (...) {
                task.promise.set_exception(std::current_exception());
            }
        }
    }

    std::vector<std::thread> workers_;
    std::deque<Task> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    bool stopping_ = false;
};

PluginManager::PluginManager()
    : physics_integrator_(std::make_unique<PhysicsIntegrator>()),
      task_scheduler_(std::make_unique<PluginTaskScheduler>(std::thread::hardware_concurrency())) {
    LOG_INFO("PluginManager initialized", "PluginManager");
}

PluginManager::~PluginManager() {
    shutdown();
}

bool PluginManager::load_plugin(const std::string& path, PluginType type) {
    // Mutex ya obtenido por load_plugins_from_config()
    LoadedPlugin plugin;
    plugin.type = type;

    std::string normalized_path = normalize_plugin_path(path);
    plugin.path = normalized_path;
    plugin.handle = nullptr;

    LOG_INFO("Loading plugin: " + normalized_path, "PluginManager");

    const auto candidates = build_plugin_candidates(normalized_path);

#ifdef _WIN32
    for (const auto& candidate : candidates) {
        LOG_DEBUG("Trying plugin path: " + candidate, "PluginManager");
        plugin.lib_handle = LoadLibraryA(candidate.c_str());
        if (plugin.lib_handle) {
            plugin.path = candidate;
            break;
        }
    }

    if (!plugin.lib_handle) {
        LOG_ERROR("Failed to load plugin library: " + normalized_path, "PluginManager");
        return false;
    }
    plugin.create_func = reinterpret_cast<decltype(plugin.create_func)>(GetProcAddress(plugin.lib_handle, "plugin_create_instance"));
    plugin.configure_func = reinterpret_cast<decltype(plugin.configure_func)>(GetProcAddress(plugin.lib_handle, "plugin_configure"));
    plugin.tick_func = reinterpret_cast<decltype(plugin.tick_func)>(GetProcAddress(plugin.lib_handle, "plugin_tick"));
    plugin.destroy_func = reinterpret_cast<decltype(plugin.destroy_func)>(GetProcAddress(plugin.lib_handle, "plugin_destroy_instance"));
    plugin.set_host_services_func = reinterpret_cast<decltype(plugin.set_host_services_func)>(GetProcAddress(plugin.lib_handle, "plugin_set_host_services"));
#else
    for (const auto& candidate : candidates) {
        LOG_DEBUG("Trying plugin path: " + candidate, "PluginManager");
        plugin.lib_handle = dlopen(candidate.c_str(), RTLD_LAZY);
        if (plugin.lib_handle) {
            plugin.path = candidate;
            break;
        }
    }

    if (!plugin.lib_handle) {
        LOG_ERROR("Failed to load plugin library: " + normalized_path + " - " + std::string(dlerror()), "PluginManager");
        return false;
    }
    plugin.create_func = reinterpret_cast<decltype(plugin.create_func)>(dlsym(plugin.lib_handle, "plugin_create_instance"));
    plugin.configure_func = reinterpret_cast<decltype(plugin.configure_func)>(dlsym(plugin.lib_handle, "plugin_configure"));
    plugin.tick_func = reinterpret_cast<decltype(plugin.tick_func)>(dlsym(plugin.lib_handle, "plugin_tick"));
    plugin.destroy_func = reinterpret_cast<decltype(plugin.destroy_func)>(dlsym(plugin.lib_handle, "plugin_destroy_instance"));
    plugin.set_host_services_func = reinterpret_cast<decltype(plugin.set_host_services_func)>(dlsym(plugin.lib_handle, "plugin_set_host_services"));
#endif

    if (plugin.create_func == nullptr || plugin.tick_func == nullptr || plugin.destroy_func == nullptr) {
        LOG_ERROR("Failed to find required plugin API functions in: " + normalized_path, "PluginManager");
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
        LOG_ERROR("Failed to create plugin instance: " + normalized_path, "PluginManager");
#ifdef _WIN32
        FreeLibrary(plugin.lib_handle);
#else
        dlclose(plugin.lib_handle);
#endif
        return false;
    }

    loaded_plugins_.emplace_back(std::move(plugin));
    LOG_INFO("Plugin loaded successfully: " + loaded_plugins_.back().path, "PluginManager");
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

        bool enable_host_logger = false;
        if (plugin_config.parameters.is_object()) {
            enable_host_logger = plugin_config.parameters.value("use_host_logger", false);
        }

        if (!loaded_plugins_.empty() && loaded_plugins_.back().set_host_services_func) {
            if (enable_host_logger) {
                loaded_plugins_.back().set_host_services_func(get_stable_host_services());
                LOG_INFO("Host logger enabled for plugin: " + plugin_config.name, "PluginManager");
            } else {
                LOG_INFO("Host logger disabled for plugin: " + plugin_config.name, "PluginManager");
            }
        }

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

    // Phase 1: sequential execution (modifies state)
    execute_sequential_plugins(state_buffer, delta_time);

    // Phase 2: parallel execution (computes forces)
    execute_parallel_plugins(state_buffer, delta_time);

    // Phase 3: physical integration with accumulated forces/torques
    apply_physics_integration(state_buffer, delta_time);

    total_cycles_++;
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    total_cycle_time_.store(total_cycle_time_.load() + duration.count() / 1000.0);
}

void PluginManager::execute_sequential_plugins(std::vector<uint8_t>& state_buffer, double delta_time) {
    std::vector<LoadedPlugin*> sequential_plugins;
    {
        std::lock_guard<std::mutex> lock(plugins_mutex_);
        for (auto& plugin : loaded_plugins_) {
            if (plugin.enabled && plugin.type == PluginType::SEQUENTIAL_STATE_MODIFIER) {
                sequential_plugins.push_back(&plugin);
            }
        }
    }

    PluginTickData tick_data = {};
    tick_data.state_buffer = state_buffer.data();
    tick_data.buffer_size = state_buffer.size();
    tick_data.delta_time = delta_time;
    tick_data.output_force = nullptr;
    tick_data.output_torque = nullptr;
    tick_data.force_out = nullptr;
    tick_data.torque_out = nullptr;

    for (auto* plugin : sequential_plugins) {
        auto start_time = std::chrono::high_resolution_clock::now();

        int32_t result = plugin->tick_func(plugin->handle, &tick_data);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        double exec_time = duration.count() / 1000.0;

        plugin->execution_count++;
        plugin->last_execution_time.store(exec_time);
        plugin->total_execution_time.store(plugin->total_execution_time.load() + exec_time);

        if (result != 0) {
            LOG_WARNING("Sequential plugin returned error code: " + std::to_string(result), "PluginManager");
        }
    }
}

void PluginManager::execute_parallel_plugins(std::vector<uint8_t>& state_buffer, double delta_time) {
    std::vector<std::future<void>> futures;
    std::vector<PluginVector3> forces;
    std::vector<PluginVector3> torques;

    auto parallel_plugins = get_plugins_by_type(PluginType::PARALLEL_PHYSICS_CALCULATOR);

    if (parallel_plugins.empty()) {
        std::lock_guard<std::mutex> lock(force_mutex_);
        accumulated_force_ = {0.0f, 0.0f, 0.0f};
        accumulated_torque_ = {0.0f, 0.0f, 0.0f};
        return;
    }

    forces.resize(parallel_plugins.size());
    torques.resize(parallel_plugins.size());

    for (size_t i = 0; i < parallel_plugins.size(); ++i) {
        auto* plugin = parallel_plugins[i];

        futures.emplace_back(task_scheduler_->submit([plugin, &state_buffer, &forces, &torques, delta_time, i]() {
            auto start_time = std::chrono::high_resolution_clock::now();

            PluginTickData tick_data = {};
            tick_data.state_buffer = state_buffer.data();
            tick_data.buffer_size = state_buffer.size();
            tick_data.delta_time = delta_time;
            tick_data.output_force = &forces[i];
            tick_data.output_torque = &torques[i];
            tick_data.force_out = &forces[i];
            tick_data.torque_out = &torques[i];

            int32_t result = plugin->tick_func(plugin->handle, &tick_data);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            double exec_time = duration.count() / 1000.0;

            plugin->execution_count++;
            plugin->last_execution_time.store(exec_time);
            plugin->total_execution_time.store(plugin->total_execution_time.load() + exec_time);

            if (result != 0) {
                LOG_WARNING("Parallel plugin returned error code: " + std::to_string(result), "PluginManager");
            }
        }));
    }

    for (auto& future : futures) {
        future.wait();
    }

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

    const state_vector::GeneralState* current_state = state_vector::GetGeneralState(state_buffer.data());
    if (!current_state) {
        LOG_ERROR("Failed to parse state buffer in physics integration", "PluginManager");
        return;
    }

    PluginVector3 plugin_force;
    PluginVector3 plugin_torque;
    {
        std::lock_guard<std::mutex> lock(force_mutex_);
        plugin_force = accumulated_force_;
        plugin_torque = accumulated_torque_;
    }

    Vector3 total_force(plugin_force.x, plugin_force.y, plugin_force.z);
    Vector3 total_torque(plugin_torque.x, plugin_torque.y, plugin_torque.z);

    // If there are no forces/torques, only advance time without rebuilding buffer
    if (total_force.isZero() && total_torque.isZero()) {

        auto* mutable_state = flatbuffers::GetMutableRoot<state_vector::GeneralState>(state_buffer.data());
        if (mutable_state) {
            float new_time = static_cast<float>(mutable_state->sim_time() + delta_time);
            mutable_state->mutate_sim_time(new_time);
        }
        return;
    }

    PhysicsState physics_state = MoLab::PhysicsIntegrator::fromFlatBuffer(current_state);
    PhysicsState new_state = physics_integrator_->integrate(physics_state, total_force, total_torque, delta_time);

    flatbuffers::FlatBufferBuilder builder;

    // Updated kinematics
    auto position = state_vector::Vec3(new_state.position.x(), new_state.position.y(), new_state.position.z());
    auto velocity = state_vector::Vec3(new_state.velocity.x(), new_state.velocity.y(), new_state.velocity.z());
    auto orientation = state_vector::Quaternion(
        static_cast<float>(new_state.orientation.x()),
        static_cast<float>(new_state.orientation.y()),
        static_cast<float>(new_state.orientation.z()),
        static_cast<float>(new_state.orientation.w()));
    auto angular_velocity = state_vector::Vec3(new_state.angular_velocity.x(), new_state.angular_velocity.y(), new_state.angular_velocity.z());

    // Mass and CG properties
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

    // Aerodynamic data - plugins update these values in the buffer
    // PluginManager only copies them from current state
    float mach_number = current_state->mach_number();
    float dynamic_pressure = current_state->dynamic_pressure();
    float angle_of_attack = current_state->angle_of_attack();
    float sideslip_angle = current_state->sideslip_angle();

    float atm_density = current_state->atm_density();
    float atm_pressure = current_state->atm_pressure();
    float atm_temperature = current_state->atm_temperature();

    // Arrays: copy if present
    flatbuffers::Offset<flatbuffers::Vector<float>> propellant_masses_fb;
    if (auto pm = current_state->propellant_masses()) {
        std::vector<float> pm_vec;
        pm_vec.reserve(pm->size());
        for (auto v : *pm) pm_vec.push_back(v);
        propellant_masses_fb = builder.CreateVector(pm_vec);
    }

    // Engines (vector of structs)
    flatbuffers::Offset<flatbuffers::Vector<const state_vector::EngineCmd*>> engines_fb;
    if (auto engines = current_state->engines()) {
        std::vector<state_vector::EngineCmd> eng_vec;
        eng_vec.reserve(engines->size());
        for (size_t i = 0; i < engines->size(); ++i) {
            const state_vector::EngineCmd* ec = engines->Get(i);
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

    // Build GeneralState while preserving dt
    state_vector::GeneralStateBuilder gs_builder(builder);
    gs_builder.add_sim_time(static_cast<float>(new_state.time));
    gs_builder.add_dt(current_state->dt());

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

    if (engines_fb.o != 0) gs_builder.add_engines(engines_fb);
    if (surface_deflections_fb.o != 0) gs_builder.add_surface_deflections(surface_deflections_fb);

    auto general_state = gs_builder.Finish();
    builder.Finish(general_state);

    // Update state buffer
    const uint8_t* new_buffer = builder.GetBufferPointer();
    uint32_t new_size = builder.GetSize();
    state_buffer.assign(new_buffer, new_buffer + new_size);

    LOG_DEBUG(
        "Physics integrated; sim_time=" + std::to_string(new_state.time) +
        " pos=(" + std::to_string(new_state.position.x()) + ", " +
        std::to_string(new_state.position.y()) + ", " + std::to_string(new_state.position.z()) + ")",
        "PluginManager"
    );
}

std::vector<LoadedPlugin*> PluginManager::get_plugins_by_type(PluginType type) {
    std::vector<LoadedPlugin*> result;
    std::lock_guard<std::mutex> lock(plugins_mutex_);
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
    task_scheduler_.reset();

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
