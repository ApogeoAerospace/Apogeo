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
    * Clase que representa el motor de simulación mejorado.
    * Contiene métodos para inicializar la simulación, cargar plugins,
    * ejecutar ticks de simulación y limpiar recursos con thread safety.
    */
    SimulationEngine();
    ~SimulationEngine() noexcept;

    // Prevenir copia y asignación
    SimulationEngine(const SimulationEngine&) = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;

    /*
    * Inicializa la simulación cargando el estado inicial desde un archivo JSON.
    * Devuelve true si la inicialización fue exitosa, false en caso contrario.
    */
    bool initialize(const std::string& state_filepath);

    /*
    * Inicializa la simulación con un archivo de configuración.
    * Carga la configuración, configura logging y plugins automáticamente.
    */
    bool initialize_with_config(const std::string& config_filepath);

    /*
    * Carga una librería dinámica de plugin desde la ruta especificada.
    */
    void load_plugin(const std::string& path, int plugin_type);

    /*
    * Corre un tick de la simulación ejecutando todos los plugins cargados.
    */
    void run_tick();

    /*
    * Ejecuta una simulación completa basada en la configuración.
    * Devuelve true si la simulación se completó exitosamente.
    */
    bool run_simulation();

    /*
    * Limpia y libera los recursos de la simulación.
    */
    void shutdown();

    // Métodos de información y estadísticas
    double get_simulation_time() const { return simulation_time_; }
    uint64_t get_iteration_count() const { return iteration_count_; }
    double get_last_tick_duration() const { return last_tick_duration_; }
    bool is_running() const { return is_running_; }

    // Validación del estado de simulación
    bool validate_simulation_state() const;

    // Métricas de rendimiento
    void print_performance_metrics() const;

private:
    // Gestor de plugins que maneja la carga y ejecución de plugins.
    std::unique_ptr<MoLab::PluginManager> plugin_manager_;

    // Buffer que contiene el estado actual de la simulación (thread-safe).
    std::vector<uint8_t> current_state_buffer_;
    mutable std::mutex state_mutex_;

    // Estado de la simulación
    std::atomic<double> simulation_time_{0.0};
    std::atomic<uint64_t> iteration_count_{0};
    std::atomic<double> last_tick_duration_{0.0};
    std::atomic<bool> is_running_{false};
};

#endif // SIMULATION_ENGINE_H
