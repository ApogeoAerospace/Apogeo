#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <mutex>
#include <memory>
#include <condition_variable>
#include <deque>
#include <thread>
#include <atomic>
#include "state_vector_generated.h"
#include "nlohmann/json.hpp"

/**
 * @file OutputManager.h
 * @brief Persistence and export management for simulation results.
 */

namespace MoLab {

/**
 * @struct SimulationDataPoint
 * @brief Serializable state sample for CSV/JSON/binary outputs.
 */
struct SimulationDataPoint {
    double time;           // Simulation time (relative)
    double utc_time;       // UTC time (absolute)

    // Kinematics
    double position_x, position_y, position_z;
    double velocity_x, velocity_y, velocity_z;
    double orientation_x, orientation_y, orientation_z, orientation_w;
    double angular_velocity_x, angular_velocity_y, angular_velocity_z;

    // Mass and properties
    double total_mass;
    double cg_x, cg_y, cg_z;

    // Aerodynamic data
    double mach_number;
    double dynamic_pressure;
    double angle_of_attack;
    double sideslip_angle;

    // Environment
    double atm_density, atm_pressure, atm_temperature;
    double gravity_x, gravity_y, gravity_z;
    double wind_speed_x, wind_speed_y, wind_speed_z;
};

/**
 * @class OutputManager
 * @brief Singleton manager for output results and metrics.
 */
class OutputManager {
public:
    /**
     * @brief Gets the global output manager instance.
     * @return Unique reference to `OutputManager`.
     */
    static OutputManager& getInstance();

    /**
     * @brief Sets output directory for result files.
     * @param dir Output directory path.
     */
    void setOutputDirectory(const std::string& dir);

    /**
     * @brief Enables or disables output formats.
     * @param csv Enable CSV output.
     * @param json Enable JSON output.
     * @param binary Enable binary output.
     */
    void setOutputFormats(bool csv, bool json, bool binary);

    /**
     * @brief Sets write interval in ticks.
     * @param interval Number of ticks between records.
     */
    void setOutputInterval(int interval); // Save every N ticks

    /**
     * @brief Records state using a FlatBuffers buffer.
     * @param state_buffer Serialized state buffer.
     * @param simulation_time Simulation time.
     * @param utc_time Current UTC time.
     * @param tick Current tick.
     */
    void recordState(const std::vector<uint8_t>& state_buffer, double simulation_time, double utc_time, int tick); // Compatibility

    /**
     * @brief Records state using a `GeneralState` pointer.
     * @param state Parsed state.
     * @param simulation_time Simulation time.
     * @param utc_time Current UTC time.
     * @param tick Current tick.
     */
    void recordState(const state_vector::GeneralState* state, double simulation_time, double utc_time, int tick);

    /**
     * @brief Records a metric value per component.
     * @param component Component name.
     * @param metric_name Metric name.
     * @param value Measured value.
     */
    void recordMetrics(const std::string& component, const std::string& metric_name, double value);

    /**
     * @brief Initializes output files and resources.
     * @param run_name Optional base run name.
     */
    void initializeOutput(const std::string& run_name = "");

    /**
     * @brief Finalizes files, flushes buffers, and closes output.
     */
    void finalizeOutput();

    /**
     * @brief Forces immediate write for open files.
     */
    void flush();

    /**
     * @brief Gets data points accumulated in memory.
     * @return Constant reference to data point container.
     */
    const std::vector<SimulationDataPoint>& getDataPoints() const { return data_points_; }

    /**
     * @brief Prints a result summary to the logger.
     */
    void printSummary();

private:
    OutputManager() = default;
    ~OutputManager() noexcept;

    OutputManager(const OutputManager&) = delete;
    OutputManager& operator=(const OutputManager&) = delete;

    /**
     * @brief Extracts an output sample from a FlatBuffers state.
     */
    SimulationDataPoint extractDataPoint(const state_vector::GeneralState* state, double time, double utc_time, int tick);

    /**
     * @brief Writes CSV column headers.
     */
    void writeCSVHeader();

    /**
     * @brief Writes one sample in CSV format.
     */
    void writeDataPointCSV(const SimulationDataPoint& point, int tick);

    /**
     * @brief Writes one sample in JSON format.
     */
    void writeDataPointJSON(const SimulationDataPoint& point, int tick);

    /**
     * @brief Writes aggregated metrics in final JSON block.
     */
    void writeMetricsJSON();

    // Configuration
    std::string output_dir_ = "output";
    bool output_csv_ = true;
    bool output_json_ = true;
    bool output_binary_ = false;
    int output_interval_ = 1; // Save each tick by default

    // In-memory data
    std::vector<SimulationDataPoint> data_points_;
    std::map<std::string, std::map<std::string, std::vector<double>>> metrics_;

    // Output files
    std::unique_ptr<std::ofstream> csv_file_;
    std::unique_ptr<std::ofstream> json_file_;
    std::unique_ptr<std::ofstream> binary_file_;
    std::string run_name_;

    // Structure for async write queue
    struct PendingPoint {
        SimulationDataPoint point;
        int tick;
    };

    // Synchronization and work queue
    std::mutex data_mutex_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<PendingPoint> pending_points_;
    std::thread writer_thread_;
    std::atomic<bool> writer_running_{false};

    // Internal state
    bool initialized_ = false;
    int last_recorded_tick_ = -1;

    /**
     * @brief Worker loop for async writer thread.
     */
    void writerLoop();
};

} // namespace MoLab
