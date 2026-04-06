#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <chrono>
#include <ctime>
#include <string>
#include <mutex>
#include <atomic>

/**
 * @file TimeManager.h
 * @brief Declaration of centralized simulation-time and UTC manager.
 */

namespace MoLab {

/**
 * @brief Centralized time manager for MoLab
 *
 * Handles both simulation time (relative) and UTC time (absolute).
 * Provides conversions across time formats and synchronization support.
 */
class TimeManager {
public:
    /**
     * @brief Gets the global time manager instance.
     * @return Unique reference to `TimeManager`.
     */
    static TimeManager& getInstance();

    /**
     * @brief Initializes simulation and UTC start times.
     * @param utc_start_time Initial UTC time in Unix seconds.
     * @param simulation_start_time Initial simulation time.
     */
    void initialize(double utc_start_time = 0.0, double simulation_start_time = 0.0);

    /**
     * @brief Sets UTC start time and resets simulation time.
     * @param utc_time New base UTC time.
     */
    void setSimulationStartTime(double utc_time);

    /**
     * @brief Increments simulation time.
     * @param delta_time Time increment.
     */
    void updateSimulationTime(double delta_time);

    /**
     * @brief Explicitly sets simulation time.
     * @param time Desired simulation time.
     */
    void setSimulationTime(double time);

    /**
     * @brief Gets current simulation time.
     * @return Relative simulation time.
     */
    double getSimulationTime() const;

    /**
     * @brief Gets current UTC time associated with simulation.
     * @return UTC time in Unix seconds.
     */
    double getCurrentUTC() const;

    /**
     * @brief Gets base UTC time for current run.
     * @return Initial UTC time.
     */
    double getStartUTC() const;

    /**
     * @brief Gets current UTC time as formatted string.
     * @return Readable time string.
     */
    std::string getCurrentUTCString() const;

    /**
     * @brief Gets initial UTC time as formatted string.
     * @return Readable time string.
     */
    std::string getStartUTCString() const;

    /**
     * @brief Converts simulation time to UTC.
     * @param sim_time Relative simulation time.
     * @return Equivalent UTC time.
     */
    double simulationTimeToUTC(double sim_time) const;

    /**
     * @brief Converts UTC time to relative simulation time.
     * @param utc_time UTC time.
     * @return Relative simulation time.
     */
    double utcToSimulationTime(double utc_time) const;

    /**
     * @brief Resets time manager using a new current UTC reference.
     */
    void reset();

    /**
     * @brief Formats a UTC timestamp for readable output.
     * @param utc_time UTC time in Unix seconds.
     * @return UTC formatted string.
     */
    std::string formatTime(double utc_time) const;

    /**
     * @brief Gets current real system UTC time.
     * @return Current Unix time in seconds with milliseconds.
     */
    double getCurrentRealTimeUTC() const;

    /**
     * @brief Explicitly locks internal time mutex.
     */
    void lock() { time_mutex_.lock(); }

    /**
     * @brief Explicitly unlocks internal time mutex.
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
