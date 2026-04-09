#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <mutex>
#include <atomic>

/**
 * @file SimulationEngine.h
 * @brief Declaration of the main MoLab simulation engine.
 */

namespace MoLab {
    class PluginManager;
}

namespace state_vector {
    struct GeneralState;
}

/**
 * @class SimulationEngine
 * @brief Main engine that orchestrates state, plugins, time, and output.
 */
namespace MoLab {

class SimulationEngine {
public:
    struct EngineStatus {
        bool running;
        uint64_t tick;
        double sim_time;
        double last_tick_duration;
        double compute_tick_duration_ms;
        double io_tick_duration_ms;
    };

    /**
     * @brief Constructs the simulation engine.
     */
    SimulationEngine();

    /**
     * @brief Destroys the simulation engine and releases resources.
     */
    ~SimulationEngine() noexcept;

    // Disable copy and assignment
    SimulationEngine(const SimulationEngine&) = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;

    /**
     * @brief Initializes simulation with an initial state file.
     * @param state_filepath Path to initial JSON state file.
     * @return `true` if initialization is successful.
     */
    bool initialize(const std::string& state_filepath);

    /**
     * @brief Initializes simulation from configuration already loaded in `ConfigManager`.
     *
     * This method does not load configuration files. The caller is responsible
     * for calling `ConfigManager::loadConfig(...)` before invoking this method.
     *
     * @return `true` if plugin bootstrap and state initialization are successful.
     */
    bool initialize_from_loaded_config();

    /**
     * @brief Loads a dynamic plugin by name/path and type.
     * @param path Plugin identifier.
     * @param plugin_type Expected plugin type.
     */
    void load_plugin(const std::string& path, int plugin_type);

    /**
     * @brief Executes a full simulation tick.
     */
    void run_tick();

    /**
     * @brief Runs the full simulation using active configuration.
     * @return `true` if simulation completes successfully.
     */
    bool run_simulation();

    /**
     * @brief Releases resources and shuts down simulation.
     */
    void shutdown();

    /**
     * @brief Gets accumulated simulation time.
     * @return Simulation time in seconds.
     */
    double get_simulation_time() const { return simulation_time_; }

    /**
     * @brief Gets number of executed iterations.
     * @return Count of processed ticks.
     */
    uint64_t get_iteration_count() const { return iteration_count_; }

    /**
     * @brief Gets the duration of the last tick.
     * @return Duration in milliseconds.
     */
    double get_last_tick_duration() const { return last_tick_duration_; }

    /**
     * @brief Gets compute-only duration of the last tick.
     * @return Duration in milliseconds.
     */
    double get_last_compute_tick_duration() const { return compute_tick_duration_ms_; }

    /**
     * @brief Gets I/O/output duration of the last tick.
     * @return Duration in milliseconds.
     */
    double get_last_io_tick_duration() const { return io_tick_duration_ms_; }

    /**
     * @brief Indicates whether simulation is currently running.
     * @return `true` if engine is active.
     */
    bool is_running() const { return is_running_; }

    /**
     * @brief Gets a read-only runtime status snapshot.
     * @return Current engine status values.
     */
    EngineStatus getStatus() const;

    /**
     * @brief Validates current internal simulation state.
     * @return `true` if state is valid.
     */
    bool validate_simulation_state() const;

    /**
     * @brief Prints simulation and plugin performance metrics.
     */
    void print_performance_metrics() const;

private:
    // Plugin manager
    std::unique_ptr<PluginManager> plugin_manager_;

    // Current state buffer (thread-safe)
    std::vector<uint8_t> current_state_buffer_;
    mutable std::mutex state_mutex_;

    // Simulation state
    std::atomic<double> simulation_time_{0.0};
    std::atomic<uint64_t> iteration_count_{0};
    std::atomic<double> last_tick_duration_{0.0};
    std::atomic<double> compute_tick_duration_ms_{0.0};
    std::atomic<double> io_tick_duration_ms_{0.0};
    std::atomic<bool> is_running_{false};

    /**
     * @brief Validates an already parsed state.
     * @param state State to validate.
     * @return `true` if state is valid.
     */
    bool validate_simulation_state(const state_vector::GeneralState* state) const;
};

} // namespace MoLab

#endif // SIMULATION_ENGINE_H
