#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "plugin_api.h"
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <future>

/**
 * @file PluginManager.h
 * @brief Management of load, execution, and lifecycle for dynamic plugins.
 */

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/**
 * @enum PluginType
 * @brief Functional role of a plugin within simulation cycle.
 */
enum class PluginType {
    SEQUENTIAL_STATE_MODIFIER = 0,
    PARALLEL_PHYSICS_CALCULATOR = 1
};

// Forward declarations
namespace MoLab {
    class PhysicsIntegrator;
    class PluginTaskScheduler;
}

namespace MoLab {

/**
 * @struct LoadedPlugin
 * @brief Represents a loaded plugin with metadata, functions, and metrics.
 */
struct LoadedPlugin {
    PluginHandle handle = nullptr;
    PluginType type = PluginType::SEQUENTIAL_STATE_MODIFIER;
    std::string path;
    std::string name;
    bool enabled = true;

    // Performance metrics
    std::atomic<uint64_t> execution_count{0};
    std::atomic<double> total_execution_time{0.0};
    std::atomic<double> last_execution_time{0.0};

    // Plugin API functions
    PluginHandle(*create_func)() = nullptr;
    int32_t(*configure_func)(PluginHandle, const char*) = nullptr;
    int32_t(*tick_func)(PluginHandle, PluginTickData*) = nullptr;
    void(*destroy_func)(PluginHandle) = nullptr;
    void(*set_host_services_func)(const PluginHostServices*) = nullptr;

#if defined(_WIN32)
    HMODULE lib_handle = nullptr;
#else
    void* lib_handle = nullptr;
#endif

    // Default constructor
    LoadedPlugin() = default;

    // Delete copy constructor and copy assignment
    LoadedPlugin(const LoadedPlugin&) = delete;
    LoadedPlugin& operator=(const LoadedPlugin&) = delete;

    // Enable move constructor and move assignment
    LoadedPlugin(LoadedPlugin&& other) noexcept
        : handle(other.handle),
          type(other.type),
          path(std::move(other.path)),
          name(std::move(other.name)),
          enabled(other.enabled),
          execution_count(other.execution_count.load()),
          total_execution_time(other.total_execution_time.load()),
          last_execution_time(other.last_execution_time.load()),
          create_func(other.create_func),
          configure_func(other.configure_func),
          tick_func(other.tick_func),
          destroy_func(other.destroy_func),
          set_host_services_func(other.set_host_services_func),
          lib_handle(other.lib_handle) {

        // Reset source object
        other.handle = nullptr;
        other.create_func = nullptr;
        other.configure_func = nullptr;
        other.tick_func = nullptr;
        other.destroy_func = nullptr;
        other.set_host_services_func = nullptr;
        other.lib_handle = nullptr;
    }

    LoadedPlugin& operator=(LoadedPlugin&& other) noexcept {
        if (this != &other) {
            handle = other.handle;
            type = other.type;
            path = std::move(other.path);
            name = std::move(other.name);
            enabled = other.enabled;
            execution_count.store(other.execution_count.load());
            total_execution_time.store(other.total_execution_time.load());
            last_execution_time.store(other.last_execution_time.load());
            create_func = other.create_func;
            configure_func = other.configure_func;
            tick_func = other.tick_func;
            destroy_func = other.destroy_func;
            set_host_services_func = other.set_host_services_func;
            lib_handle = other.lib_handle;

            // Reset source object
            other.handle = nullptr;
            other.create_func = nullptr;
            other.configure_func = nullptr;
            other.tick_func = nullptr;
            other.destroy_func = nullptr;
            other.set_host_services_func = nullptr;
            other.lib_handle = nullptr;
        }
        return *this;
    }
};

/**
 * @class PluginManager
 * @brief Manages plugin lifecycle and execution with thread safety.
 */
class PluginManager {
public:
    /**
     * @brief Constructs manager and initializes auxiliary subsystems.
     */
    PluginManager();

    /**
     * @brief Destroys manager and releases associated resources.
     */
    ~PluginManager();

    // Prevent copy and assignment
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /**
     * @brief Loads a dynamic plugin and classifies it by type.
     * @param path Plugin library path.
     * @param type Plugin type to register.
     * @return `true` if plugin is loaded successfully.
     */
    bool load_plugin(const std::string& path, PluginType type, bool enable_host_logger = false);

    /**
     * @brief Loads plugins defined in global configuration.
     * @return `true` if all enabled plugins were loaded successfully.
     */
    bool load_plugins_from_config();

    /**
     * @brief Runs a complete plugin cycle and physical integration.
     * @param state_buffer Serialized state buffer.
     * @param delta_time Tick time step.
     */
    void run_simulation_cycle(std::vector<uint8_t>& state_buffer, double delta_time);

    /**
     * @brief Releases all resources and unloads all plugins.
     */
    void shutdown();

    /**
     * @brief Gets number of loaded plugins.
     * @return Number of registered plugins.
     */
    size_t get_plugin_count() const;

    /**
     * @brief Gets names of loaded plugins.
     * @return List of plugin names or paths.
     */
    std::vector<std::string> get_loaded_plugin_names() const;

    /**
     * @struct PluginMetrics
     * @brief Accumulated execution metrics per plugin.
     */
    struct PluginMetrics {
        std::string name;
        uint64_t execution_count;
        double total_execution_time;
        double average_execution_time;
        double last_execution_time;
    };

    /**
     * @brief Gets performance metrics for loaded plugins.
     * @return Vector of metrics per plugin.
     */
    std::vector<PluginMetrics> get_plugin_metrics() const;

private:
    // Plugin containers with concurrency protection
    std::vector<LoadedPlugin> loaded_plugins_;
    mutable std::mutex plugins_mutex_;

    // Global simulation-cycle metrics
    std::atomic<uint64_t> total_cycles_{0};
    std::atomic<double> total_cycle_time_{0.0};

    // Internal execution methods
    void execute_sequential_plugins(std::vector<uint8_t>& state_buffer, double delta_time);
    void execute_parallel_plugins(std::vector<uint8_t>& state_buffer, double delta_time);
    void apply_physics_integration(std::vector<uint8_t>& state_buffer, double delta_time);

    // Utilities
    std::vector<LoadedPlugin*> get_plugins_by_type(PluginType type);
    void cleanup_plugin(LoadedPlugin& plugin);

    // Physics integrator
    std::unique_ptr<MoLab::PhysicsIntegrator> physics_integrator_;

    // Task scheduler for parallel plugins
    std::unique_ptr<PluginTaskScheduler> task_scheduler_;

    // Force/torque accumulators (parallel plugins)
    PluginVector3 accumulated_force_{0.0f, 0.0f, 0.0f};
    PluginVector3 accumulated_torque_{0.0f, 0.0f, 0.0f};
    mutable std::mutex force_mutex_;
};

} // namespace MoLab

#endif
