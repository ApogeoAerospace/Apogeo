#ifndef SIMULATION_ENGINE_H
#define SIMULATION_ENGINE_H

#include <string>
#include <memory>
#include <vector>
#include <cstdint>

class PluginManager;

class SimulationEngine {
public:
    /*
    * Clase que representa el motor de simulación.
    * Contiene métodos para inicializar la simulación, cargar plugins,
    * ejecutar ticks de simulación y limpiar recursos.
    */
    SimulationEngine();
    ~SimulationEngine();

    /*
    * Inicializa la simulación cargando el estado inicial desde un archivo JSON.
    * Devuelve true si la inicialización fue exitosa, false en caso contrario.
    */
    bool initialize(const std::string& state_filepath);

    /*
    * Carga una librería dinámica de plugin desde la ruta especificada.
    */
    void load_plugin(const std::string& path, int plugin_type);

    /*
    * Corre un tick de la simulación ejecutando todos los plugins cargados.
    */
    void run_tick();

    /*
    * Limpia y libera los recursos de la simulación.
    */
    void shutdown();

private:
    // Gestor de plugins que maneja la carga y ejecución de plugins.
    std::unique_ptr<PluginManager> plugin_manager_;
    // Buffer que contiene el estado actual de la simulación.
    std::vector<uint8_t> current_state_buffer_;
};

#endif // SIMULATION_ENGINE_H
