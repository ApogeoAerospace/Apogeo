#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <chrono>
#include <ctime>
#include <string>
#include <mutex>
#include <atomic>

namespace MoLab {

/**
 * @brief Gestor centralizado de tiempo para MoLab
 *
 * Maneja tanto el tiempo de simulación (relativo) como el tiempo UTC (absoluto).
 * Proporciona conversiones entre diferentes formatos de tiempo y sincronización.
 */
class TimeManager {
public:
    static TimeManager& getInstance();

    // Configuración inicial
    void initialize(double utc_start_time = 0.0, double simulation_start_time = 0.0);
    void setSimulationStartTime(double utc_time);

    // Tiempo de simulación (relativo)
    void updateSimulationTime(double delta_time);
    void setSimulationTime(double time);
    double getSimulationTime() const;

    // Tiempo UTC (absoluto)
    double getCurrentUTC() const;
    double getStartUTC() const;
    std::string getCurrentUTCString() const;
    std::string getStartUTCString() const;

    // Conversiones
    double simulationTimeToUTC(double sim_time) const;
    double utcToSimulationTime(double utc_time) const;

    // Utilidades
    void reset();
    std::string formatTime(double utc_time) const;
    double getCurrentRealTimeUTC() const;

    // Thread safety
    void lock() { time_mutex_.lock(); }
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
