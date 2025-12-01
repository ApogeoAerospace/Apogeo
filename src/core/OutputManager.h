#pragma once

#include <string>
#include <fstream>
#include <vector>
#include <mutex>
#include <memory>
#include "state_vector_generated.h"
#include "nlohmann/json.hpp"

namespace MoLab {

struct SimulationDataPoint {
    double time;           // Simulation time (relative)
    double utc_time;       // UTC time (absolute)
    double position_x, position_y, position_z;
    double velocity_x, velocity_y, velocity_z;
    double orientation_x, orientation_y, orientation_z, orientation_w;
    double atm_density, atm_pressure, atm_temperature;
    double gravity_x, gravity_y, gravity_z;
    double wind_speed_x, wind_speed_y, wind_speed_z;
};

class OutputManager {
public:
    static OutputManager& getInstance();

    // Configuration
    void setOutputDirectory(const std::string& dir);
    void setOutputFormats(bool csv, bool json, bool binary);
    void setOutputInterval(int interval); // Save every N ticks

    // Data recording
    void recordState(const std::vector<uint8_t>& state_buffer, double simulation_time, double utc_time, int tick);
    void recordMetrics(const std::string& component, const std::string& metric_name, double value);

    // File operations
    void initializeOutput(const std::string& run_name = "");
    void finalizeOutput();
    void flush();

    // Data access
    const std::vector<SimulationDataPoint>& getDataPoints() const { return data_points_; }
    void printSummary();

private:
    OutputManager() = default;
    ~OutputManager() noexcept;

    OutputManager(const OutputManager&) = delete;
    OutputManager& operator=(const OutputManager&) = delete;

    // Helper methods
    SimulationDataPoint extractDataPoint(const state_vector::GeneralState* state, double time, double utc_time, int tick);
    void writeCSVHeader();
    void writeDataPointCSV(const SimulationDataPoint& point, int tick);
    void writeDataPointJSON(const SimulationDataPoint& point, int tick);
    void writeMetricsJSON();

    // Configuration
    std::string output_dir_ = "output";
    bool output_csv_ = true;
    bool output_json_ = true;
    bool output_binary_ = false;
    int output_interval_ = 1; // Save every tick by default

    // Data storage
    std::vector<SimulationDataPoint> data_points_;
    std::map<std::string, std::map<std::string, std::vector<double>>> metrics_;

    // File handles
    std::unique_ptr<std::ofstream> csv_file_;
    std::unique_ptr<std::ofstream> json_file_;
    std::unique_ptr<std::ofstream> binary_file_;
    std::string run_name_;

    // Thread safety
    std::mutex data_mutex_;

    // State tracking
    bool initialized_ = false;
    int last_recorded_tick_ = -1;
};

} // namespace MoLab
