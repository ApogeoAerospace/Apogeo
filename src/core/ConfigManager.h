#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

/**
 * @file ConfigManager.h
 * @brief Declaration of the central MoLab configuration manager.
 */

// Main namespace for MoLab core.
namespace MoLab {

/**
 * @struct PluginConfig
 * @brief Configuration model for a loadable plugin.
 */
struct PluginConfig {
    std::string name;
    int type;
    std::string library_path;
    bool enabled;
    nlohmann::json parameters;
};

/**
 * @struct SimulationConfig
 * @brief General simulation configuration model.
 */
struct SimulationConfig {
    double simulation_duration;
    int max_iterations;
    bool enable_logging;
    std::string log_file;
    std::string log_level;
};

/**
 * @class ConfigManager
 * @brief Centralized JSON configuration manager.
 *
 * Implements a singleton pattern to load, validate, and expose
 * simulation parameters, plugins, and auxiliary paths.
 */
class ConfigManager {
public:
    /**
     * @brief Gets the global manager instance.
     *
     * @return Unique reference to `ConfigManager`.
     */
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    /**
     * @brief Loads configuration from a JSON file.
     *
     * @param config_file Configuration file path.
     * @return `true` if load and validation are successful.
     */
    bool loadConfig(const std::string& config_file);

    /**
     * @brief Saves current configuration to a JSON file.
     *
     * @param config_file Destination file path.
     * @return `true` if write is successful.
     */
    bool saveConfig(const std::string& config_file) const;

    /**
     * @brief Gets the current simulation configuration.
     * @return Constant reference to simulation configuration.
     */
    const SimulationConfig& getSimulationConfig() const { return simulation_config_; }

    /**
     * @brief Gets loaded plugin configurations.
     * @return List of plugin configurations.
     */
    const std::vector<PluginConfig>& getPluginConfigs() const { return plugin_configs_; }

    /**
     * @brief Gets the initial state file path.
     * @return Configured initial state path.
     */
    std::string getInitialStateFile() const { return initial_state_file_; }

    /**
     * @brief Gets configured output directory.
     * @return Output directory path.
     */
    std::string getOutputDirectory() const { return output_directory_; }

    /**
     * @brief Replaces the current simulation configuration.
     * @param config New simulation configuration.
     */
    void setSimulationConfig(const SimulationConfig& config) { simulation_config_ = config; }

    /**
     * @brief Adds a plugin configuration to the active collection.
     * @param config Plugin configuration to add.
     */
    void addPluginConfig(const PluginConfig& config) { plugin_configs_.push_back(config); }

    /**
     * @brief Validates current configuration consistency.
     * @return `true` if parameters are valid.
     */
    bool validateConfig() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    /**
     * @brief Sets default values for all configuration fields.
     */
    void setDefaults();

    /**
     * @brief Parses the `simulation` section from JSON.
     * @param json Configuration JSON document.
     * @return `true` if section was processed successfully.
     */
    bool parseSimulationConfig(const nlohmann::json& json);

    /**
     * @brief Parses the `plugins` section from JSON.
     * @param json Configuration JSON document.
     * @return `true` if section was processed successfully.
     */
    bool parsePluginConfigs(const nlohmann::json& json);

    SimulationConfig simulation_config_;
    std::vector<PluginConfig> plugin_configs_;
    std::string initial_state_file_;
    std::string output_directory_;
};

} // namespace MoLab
