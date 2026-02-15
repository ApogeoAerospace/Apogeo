#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

// Espacio de nombres principal del núcleo MoLab.
namespace MoLab {

// Configuración de un plugin cargable desde archivo de configuración.
struct PluginConfig {
    std::string name;
    int type;
    std::string library_path;
    bool enabled;
    nlohmann::json parameters;
};

// Configuración general de la simulación.
struct SimulationConfig {
    double simulation_duration;
    int max_iterations;
    bool enable_logging;
    std::string log_file;
    std::string log_level;
};

// Gestor centralizado de configuración (singleton).
// Carga, valida y expone valores de simulación y plugins.
class ConfigManager {
public:
    // Acceso global a la instancia.
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    // Carga configuración desde archivo (JSON).
    bool loadConfig(const std::string& config_file);
    // Guarda configuración actual a archivo (JSON).
    bool saveConfig(const std::string& config_file) const;

    // Getters
    const SimulationConfig& getSimulationConfig() const { return simulation_config_; }
    const std::vector<PluginConfig>& getPluginConfigs() const { return plugin_configs_; }

    std::string getInitialStateFile() const { return initial_state_file_; }
    std::string getOutputDirectory() const { return output_directory_; }

    // Setters
    void setSimulationConfig(const SimulationConfig& config) { simulation_config_ = config; }
    void addPluginConfig(const PluginConfig& config) { plugin_configs_.push_back(config); }

    // Validation
    bool validateConfig() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // Establece valores por defecto cuando no hay configuración válida.
    void setDefaults();
    // Parseo de sección de simulación.
    bool parseSimulationConfig(const nlohmann::json& json);
    // Parseo de sección de plugins.
    bool parsePluginConfigs(const nlohmann::json& json);

    SimulationConfig simulation_config_;
    std::vector<PluginConfig> plugin_configs_;
    std::string initial_state_file_;
    std::string output_directory_;
};

} // namespace MoLab
