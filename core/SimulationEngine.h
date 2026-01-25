#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <mutex>
#include <atomic>

namespace MoLab {
    class PluginManager;
}

class SimulationEngine {
public:
    /*
    * Class that represents the improved simulation engine.
    * Contains methods to initialize simulation, load plugins,
    * run simulation ticks and cleanup resources with thread safety.
    */
    SimulationEngine();
    ~SimulationEngine() noexcept;

    // Prevent copy and assignment
    SimulationEngine(const SimulationEngine&) = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;

    /*
    * Initializes the simulation loading initial state from a JSON file.
    * Returns true if initialization was successful, false otherwise.
    */
    bool initialize(const std::string& state_filepath);

    /*
    * Initializes the simulation with a configuration file.
    * Loads configuration, sets up logging and plugins automatically.
    */
    bool initialize_with_config(const std::string& config_filepath);

    /*
    * Loads a dynamic plugin library from the specified path.
    */
    void load_plugin(const std::string& path, int plugin_type);

    /*
    * Runs a simulation tick executing all loaded plugins.
    */
    void run_tick();

    /*
    * Executes a complete simulation based on configuration.
    * Returns true if simulation completed successfully.
    */
    bool run_simulation();

    /*
    * Cleans up and releases simulation resources.
    */
    void shutdown();

    // Information and statistics methods
    double get_simulation_time() const { return simulation_time_; }
    uint64_t get_iteration_count() const { return iteration_count_; }
    double get_last_tick_duration() const { return last_tick_duration_; }
    bool is_running() const { return is_running_; }

    // Simulation state validation
    bool validate_simulation_state() const;

    // Performance metrics
    void print_performance_metrics() const;

private:
    // Plugin manager that handles plugin loading and execution.
    std::unique_ptr<MoLab::PluginManager> plugin_manager_;

    // Buffer containing current simulation state (thread-safe).
    std::vector<uint8_t> current_state_buffer_;
    mutable std::mutex state_mutex_;

    // Simulation state
    std::atomic<double> simulation_time_{0.0};
    std::atomic<uint64_t> iteration_count_{0};
    std::atomic<double> last_tick_duration_{0.0};
    std::atomic<bool> is_running_{false};
};

#endif // SIMULATION_ENGINE_H
