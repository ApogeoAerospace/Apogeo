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
 * @brief Declaración del motor principal de simulación de MoLab.
 */

namespace MoLab {
    class PluginManager;
}

namespace state_vector {
    struct GeneralState;
}

/**
 * @class SimulationEngine
 * @brief Motor principal que orquesta estado, plugins, tiempo y salida.
 */
namespace MoLab {

class SimulationEngine {
public:
    /**
     * @brief Construye el motor de simulación.
     */
    SimulationEngine();

    /**
     * @brief Destruye el motor de simulación y libera recursos.
     */
    ~SimulationEngine() noexcept;

    // Evitar copia y asignación
    SimulationEngine(const SimulationEngine&) = delete;
    SimulationEngine& operator=(const SimulationEngine&) = delete;

    /**
     * @brief Inicializa la simulación con un archivo de estado inicial.
     * @param state_filepath Ruta al archivo de estado JSON.
     * @return `true` si la inicialización fue exitosa.
     */
    bool initialize(const std::string& state_filepath);

    /**
     * @brief Inicializa la simulación desde un archivo de configuración.
     * @param config_filepath Ruta al archivo de configuración JSON.
     * @return `true` si la inicialización fue exitosa.
     */
    bool initialize_with_config(const std::string& config_filepath);

    /**
     * @brief Carga un plugin dinámico por nombre/ruta y tipo.
     * @param path Identificador del plugin.
     * @param plugin_type Tipo de plugin esperado.
     */
    void load_plugin(const std::string& path, int plugin_type);

    /**
     * @brief Ejecuta un tick completo de simulación.
     */
    void run_tick();

    /**
     * @brief Ejecuta la simulación completa según la configuración activa.
     * @return `true` si la simulación termina correctamente.
     */
    bool run_simulation();

    /**
     * @brief Libera recursos y cierra la simulación.
     */
    void shutdown();

    /**
     * @brief Obtiene el tiempo de simulación acumulado.
     * @return Tiempo de simulación en segundos.
     */
    double get_simulation_time() const { return simulation_time_; }

    /**
     * @brief Obtiene el número de iteraciones ejecutadas.
     * @return Cantidad de ticks procesados.
     */
    uint64_t get_iteration_count() const { return iteration_count_; }

    /**
     * @brief Obtiene la duración del último tick.
     * @return Duración en milisegundos.
     */
    double get_last_tick_duration() const { return last_tick_duration_; }

    /**
     * @brief Indica si la simulación se encuentra en ejecución.
     * @return `true` si el motor está activo.
     */
    bool is_running() const { return is_running_; }

    /**
     * @brief Valida el estado interno de simulación actual.
     * @return `true` si el estado es válido.
     */
    bool validate_simulation_state() const;

    /**
     * @brief Imprime métricas de rendimiento de simulación y plugins.
     */
    void print_performance_metrics() const;

private:
    // Gestor de plugins
    std::unique_ptr<PluginManager> plugin_manager_;

    // Buffer de estado actual (thread-safe)
    std::vector<uint8_t> current_state_buffer_;
    mutable std::mutex state_mutex_;

    // Estado de simulación
    std::atomic<double> simulation_time_{0.0};
    std::atomic<uint64_t> iteration_count_{0};
    std::atomic<double> last_tick_duration_{0.0};
    std::atomic<bool> is_running_{false};

    /**
     * @brief Valida un estado ya parseado.
     * @param state Estado a validar.
     * @return `true` si el estado es válido.
     */
    bool validate_simulation_state(const state_vector::GeneralState* state) const;
};

} // namespace MoLab

#endif // SIMULATION_ENGINE_H
