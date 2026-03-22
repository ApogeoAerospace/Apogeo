#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <chrono>
#include <ctime>
#include <string>
#include <mutex>
#include <atomic>

/**
 * @file TimeManager.h
 * @brief Declaración del gestor centralizado de tiempo de simulación y UTC.
 */

namespace MoLab {

/**
 * @brief Gestor centralizado de tiempo para MoLab
 *
 * Maneja tanto el tiempo de simulación (relativo) como el tiempo UTC (absoluto).
 * Proporciona conversiones entre diferentes formatos de tiempo y sincronización.
 */
class TimeManager {
public:
    /**
     * @brief Obtiene la instancia global del gestor de tiempo.
     * @return Referencia única a `TimeManager`.
     */
    static TimeManager& getInstance();

    /**
     * @brief Inicializa tiempos de arranque de simulación y UTC.
     * @param utc_start_time Tiempo UTC inicial en segundos Unix.
     * @param simulation_start_time Tiempo inicial de simulación.
     */
    void initialize(double utc_start_time = 0.0, double simulation_start_time = 0.0);

    /**
     * @brief Fija el tiempo UTC de inicio y reinicia el tiempo simulado.
     * @param utc_time Nuevo tiempo UTC base.
     */
    void setSimulationStartTime(double utc_time);

    /**
     * @brief Incrementa el tiempo de simulación.
     * @param delta_time Incremento temporal.
     */
    void updateSimulationTime(double delta_time);

    /**
     * @brief Establece explícitamente el tiempo de simulación.
     * @param time Tiempo de simulación deseado.
     */
    void setSimulationTime(double time);

    /**
     * @brief Obtiene el tiempo de simulación actual.
     * @return Tiempo relativo de simulación.
     */
    double getSimulationTime() const;

    /**
     * @brief Obtiene el tiempo UTC actual asociado a la simulación.
     * @return Tiempo UTC en segundos Unix.
     */
    double getCurrentUTC() const;

    /**
     * @brief Obtiene el tiempo UTC base de la ejecución.
     * @return Tiempo UTC inicial.
     */
    double getStartUTC() const;

    /**
     * @brief Obtiene el tiempo UTC actual formateado.
     * @return Cadena de tiempo legible.
     */
    std::string getCurrentUTCString() const;

    /**
     * @brief Obtiene el tiempo UTC inicial formateado.
     * @return Cadena de tiempo legible.
     */
    std::string getStartUTCString() const;

    /**
     * @brief Convierte tiempo de simulación a UTC.
     * @param sim_time Tiempo relativo de simulación.
     * @return Tiempo UTC equivalente.
     */
    double simulationTimeToUTC(double sim_time) const;

    /**
     * @brief Convierte tiempo UTC a tiempo relativo de simulación.
     * @param utc_time Tiempo UTC.
     * @return Tiempo relativo de simulación.
     */
    double utcToSimulationTime(double utc_time) const;

    /**
     * @brief Reinicia el gestor de tiempo con una nueva referencia UTC actual.
     */
    void reset();

    /**
     * @brief Formatea un timestamp UTC para salida legible.
     * @param utc_time Tiempo UTC en segundos Unix.
     * @return Cadena formateada en UTC.
     */
    std::string formatTime(double utc_time) const;

    /**
     * @brief Obtiene el tiempo UTC real del sistema.
     * @return Tiempo actual en segundos Unix con milisegundos.
     */
    double getCurrentRealTimeUTC() const;

    /**
     * @brief Bloquea explícitamente el mutex interno de tiempo.
     */
    void lock() { time_mutex_.lock(); }

    /**
     * @brief Desbloquea explícitamente el mutex interno de tiempo.
     */
    void unlock() { time_mutex_.unlock(); }

private:
    TimeManager() = default;
    ~TimeManager() = default;
    TimeManager(const TimeManager&) = delete;
    TimeManager& operator=(const TimeManager&) = delete;

    std::atomic<double> simulation_time_{0.0};
    std::atomic<double> utc_start_time_{0.0};
    mutable std::mutex time_mutex_;
    bool initialized_{false};
};

} // namespace MoLab

#endif // TIME_MANAGER_H
