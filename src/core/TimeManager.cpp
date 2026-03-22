#include "TimeManager.h"
#include "Logger.h"
#include <sstream>
#include <iomanip>

/**
 * @file TimeManager.cpp
 * @brief Implementación del gestor temporal de simulación y UTC.
 */

namespace MoLab {

TimeManager& TimeManager::getInstance() {
    static TimeManager instance;
    return instance;
}

void TimeManager::initialize(double utc_start_time, double simulation_start_time) {
    std::lock_guard<std::mutex> lock(time_mutex_);

    if (utc_start_time <= 0.0) {
        // Si no se proporciona UTC, usar el tiempo actual
        utc_start_time = getCurrentRealTimeUTC();
    }

    utc_start_time_.store(utc_start_time);
    simulation_time_.store(simulation_start_time);
    initialized_ = true;

    LOG_INFO("TimeManager initialized", "TimeManager");
    LOG_INFO("UTC start time: " + formatTime(utc_start_time), "TimeManager");
    LOG_INFO("Simulation start time: " + std::to_string(simulation_start_time) + "s", "TimeManager");
}

void TimeManager::setSimulationStartTime(double utc_time) {
    std::lock_guard<std::mutex> lock(time_mutex_);
    utc_start_time_.store(utc_time);
    simulation_time_.store(0.0);

    LOG_INFO("Simulation start time set to UTC: " + formatTime(utc_time), "TimeManager");
}

void TimeManager::updateSimulationTime(double delta_time) {
    simulation_time_.store(simulation_time_.load() + delta_time);
}

void TimeManager::setSimulationTime(double time) {
    simulation_time_.store(time);
}

double TimeManager::getSimulationTime() const {
    return simulation_time_.load();
}

double TimeManager::getCurrentUTC() const {
    return utc_start_time_.load() + simulation_time_.load();
}

double TimeManager::getStartUTC() const {
    return utc_start_time_.load();
}

std::string TimeManager::getCurrentUTCString() const {
    return formatTime(getCurrentUTC());
}

std::string TimeManager::getStartUTCString() const {
    return formatTime(getStartUTC());
}

double TimeManager::simulationTimeToUTC(double sim_time) const {
    return utc_start_time_.load() + sim_time;
}

double TimeManager::utcToSimulationTime(double utc_time) const {
    return utc_time - utc_start_time_.load();
}

void TimeManager::reset() {
    std::lock_guard<std::mutex> lock(time_mutex_);
    simulation_time_.store(0.0);
    utc_start_time_.store(getCurrentRealTimeUTC());

    LOG_INFO("TimeManager reset", "TimeManager");
}

std::string TimeManager::formatTime(double utc_time) const {
    std::time_t time_t_val = static_cast<std::time_t>(utc_time);

    // Calcular milisegundos
    double fractional_seconds = utc_time - static_cast<double>(time_t_val);
    int milliseconds = static_cast<int>(fractional_seconds * 1000.0);

    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_val), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << milliseconds << " UTC";

    return ss.str();
}

double TimeManager::getCurrentRealTimeUTC() const {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration);
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration) -
                       std::chrono::duration_cast<std::chrono::milliseconds>(seconds);

    return static_cast<double>(seconds.count()) + static_cast<double>(milliseconds.count()) / 1000.0;
}

} // namespace MoLab
