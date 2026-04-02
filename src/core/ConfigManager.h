#pragma once

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

/**
 * @file ConfigManager.h
 * @brief Declaración del gestor central de configuración de MoLab.
 */

// Espacio de nombres principal del núcleo MoLab.
namespace MoLab {

/**
 * @struct PluginConfig
 * @brief Modelo de configuración de un plugin cargable.
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
 * @brief Modelo de configuración general de simulación.
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
 * @brief Gestor centralizado de configuración en formato JSON.
 *
 * Implementa un patrón singleton para cargar, validar y exponer
 * parámetros de simulación, plugins y rutas auxiliares.
 */
class ConfigManager {
public:
    /**
     * @brief Obtiene la instancia global del gestor.
     *
     * @return Referencia única a `ConfigManager`.
     */
    static ConfigManager& getInstance() {
        static ConfigManager instance;
        return instance;
    }

    /**
     * @brief Carga configuración desde archivo JSON.
     *
     * @param config_file Ruta del archivo de configuración.
     * @return `true` si la carga y validación fueron exitosas.
     */
    bool loadConfig(const std::string& config_file);

    /**
     * @brief Guarda la configuración actual en archivo JSON.
     *
     * @param config_file Ruta de destino.
     * @return `true` si la escritura fue exitosa.
     */
    bool saveConfig(const std::string& config_file) const;

    /**
     * @brief Obtiene la configuración de simulación actual.
     * @return Referencia constante a la configuración de simulación.
     */
    const SimulationConfig& getSimulationConfig() const { return simulation_config_; }

    /**
     * @brief Obtiene la configuración de plugins cargada.
     * @return Lista de configuraciones de plugin.
     */
    const std::vector<PluginConfig>& getPluginConfigs() const { return plugin_configs_; }

    /**
     * @brief Obtiene la ruta del archivo de estado inicial.
     * @return Ruta configurada del estado inicial.
     */
    std::string getInitialStateFile() const { return initial_state_file_; }

    /**
     * @brief Obtiene el directorio de salida configurado.
     * @return Ruta del directorio de salida.
     */
    std::string getOutputDirectory() const { return output_directory_; }

    /**
     * @brief Reemplaza la configuración de simulación actual.
     * @param config Nueva configuración de simulación.
     */
    void setSimulationConfig(const SimulationConfig& config) { simulation_config_ = config; }

    /**
     * @brief Agrega una configuración de plugin a la colección activa.
     * @param config Configuración del plugin a añadir.
     */
    void addPluginConfig(const PluginConfig& config) { plugin_configs_.push_back(config); }

    /**
     * @brief Valida la coherencia de la configuración actual.
     * @return `true` si los parámetros son válidos.
     */
    bool validateConfig() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    /**
     * @brief Establece valores por defecto para toda la configuración.
     */
    void setDefaults();

    /**
     * @brief Parsea la sección `simulation` del JSON.
     * @param json Documento JSON de configuración.
     * @return `true` si la sección se procesó correctamente.
     */
    bool parseSimulationConfig(const nlohmann::json& json);

    /**
     * @brief Parsea la sección `plugins` del JSON.
     * @param json Documento JSON de configuración.
     * @return `true` si la sección se procesó correctamente.
     */
    bool parsePluginConfigs(const nlohmann::json& json);

    SimulationConfig simulation_config_;
    std::vector<PluginConfig> plugin_configs_;
    std::string initial_state_file_;
    std::string output_directory_;
};

} // namespace MoLab
