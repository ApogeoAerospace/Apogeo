#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

class PluginManager;

class SimulationEngine {
public:
    SimulationEngine();
    ~SimulationEngine();

    bool initialize(const std::string& state_filepath);
    void load_plugin(const std::string& path);
    void run_tick();
    void shutdown();

private:
    std::unique_ptr<PluginManager> plugin_manager_;
    std::vector<uint8_t> current_state_buffer_;
};

#endif // SIMULATION_ENGINE_H
