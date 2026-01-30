#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace MoLab {

struct PluginConfig {
    std::string name;
    int type;
    std::string library_path;
    bool enabled;
    nlohmann::json parameters;
};

struct SimulationConfig {
    double time_step;
    double simulation_duration;
    int max_iterations;
    bool enable_logging;
    std::string log_file;
    std::string log_level;
};

struct PhysicsConfig {
    double integration_tolerance;
    std::string integrator_type; // "euler", "runge_kutta_4", "verlet"
};

class ConfigManager {
public:
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    bool loadConfig(const std::string& config_file);
    bool saveConfig(const std::string& config_file) const;

    // Getters
    const SimulationConfig& getSimulationConfig() const { return simulation_config_; }
    const PhysicsConfig& getPhysicsConfig() const { return physics_config_; }
    const std::vector<PluginConfig>& getPluginConfigs() const { return plugin_configs_; }

    std::string getInitialStateFile() const { return initial_state_file_; }
    std::string getOutputDirectory() const { return output_directory_; }

    // Setters
    void setSimulationConfig(const SimulationConfig& config) { simulation_config_ = config; }
    void setPhysicsConfig(const PhysicsConfig& config) { physics_config_ = config; }
    void addPluginConfig(const PluginConfig& config) { plugin_configs_.push_back(config); }

    // Validation
    bool validateConfig() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    void setDefaults();
    bool parseSimulationConfig(const nlohmann::json& json);
    bool parsePhysicsConfig(const nlohmann::json& json);
    bool parsePluginConfigs(const nlohmann::json& json);

    SimulationConfig simulation_config_;
    PhysicsConfig physics_config_;
    std::vector<PluginConfig> plugin_configs_;
    std::string initial_state_file_;
    std::string output_directory_;
};

} // namespace MoLab
