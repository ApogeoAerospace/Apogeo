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

namespace state_vector {
    struct GeneralState;
}

// Motor principal de simulación.
// Orquesta carga de estado, ejecución de ticks y gestión de plugins.
class SimulationEngine {
public:
    /*
    * Clase que representa el motor de simulación mejorado.
    * Contiene métodos para inicializar, cargar plugins,
    * ejecutar ticks y limpiar recursos con seguridad de hilos.
    */
    SimulationEngine();
    ~SimulationEngine() noexcept;

    // Evitar copia y asignación
    SimulationEngine(const SimulationEngine&) = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;

    /*
    * Inicializa la simulación cargando el estado inicial desde JSON.
    * Returns true if initialization was successful, false otherwise.
    */
    bool initialize(const std::string& state_filepath);

    /*
    * Inicializa la simulación desde archivo de configuración.
    * Loads configuration, sets up logging and plugins automatically.
    */
    bool initialize_with_config(const std::string& config_filepath);

    /*
    * Carga un plugin dinámico desde ruta.
    */
    void load_plugin(const std::string& path, int plugin_type);

    /*
    * Ejecuta un tick completo de simulación.
    */
    void run_tick();

    /*
    * Ejecuta una simulación completa según configuración.
    * Returns true if simulation completed successfully.
    */
    bool run_simulation();

    /*
    * Libera recursos y cierra la simulación.
    */
    void shutdown();

    // Información de estado
    double get_simulation_time() const { return simulation_time_; }
    uint64_t get_iteration_count() const { return iteration_count_; }
    double get_last_tick_duration() const { return last_tick_duration_; }
    bool is_running() const { return is_running_; }

    // Validación del estado
    bool validate_simulation_state() const;

    // Métricas de rendimiento
    void print_performance_metrics() const;

private:
    // Gestor de plugins
    std::unique_ptr<MoLab::PluginManager> plugin_manager_;

    // Buffer de estado actual (thread-safe)
    std::vector<uint8_t> current_state_buffer_;
    mutable std::mutex state_mutex_;

    // Estado de simulación
    std::atomic<double> simulation_time_{0.0};
    std::atomic<uint64_t> iteration_count_{0};
    std::atomic<double> last_tick_duration_{0.0};
    std::atomic<bool> is_running_{false};

    // Validación interna con estado pre‑parseado
    bool validate_simulation_state(const state_vector::GeneralState* state) const;
};

#endif // SIMULATION_ENGINE_H
