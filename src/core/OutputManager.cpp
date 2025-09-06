#include "OutputManager.h"
#include "Logger.h"
#include "TimeManager.h"
#include "state_vector_generated.h"
#include <filesystem>
#include <iomanip>
#include <cmath>
#include <sstream>
#include <chrono>

using namespace MoLab;

OutputManager& OutputManager::getInstance() {
    static OutputManager instance;
    return instance;
}

OutputManager::~OutputManager() {
    if (initialized_) {
        finalizeOutput();
    }
}

void OutputManager::setOutputDirectory(const std::string& dir) {
    output_dir_ = dir;
}

void OutputManager::setOutputFormats(bool csv, bool json, bool binary) {
    output_csv_ = csv;
    output_json_ = json;
    output_binary_ = binary;
}

void OutputManager::setOutputInterval(int interval) {
    output_interval_ = std::max(1, interval);
}

void OutputManager::initializeOutput(const std::string& run_name) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    // Create timestamp for unique run identification using TimeManager
    auto& time_manager = TimeManager::getInstance();
    double current_utc = time_manager.getCurrentRealTimeUTC();
    std::time_t time_t_val = static_cast<std::time_t>(current_utc);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_val), "%Y%m%d_%H%M%S");
    
    run_name_ = run_name.empty() ? "simulation_" + ss.str() : run_name + "_" + ss.str();
    
    // Create output directory
    std::filesystem::create_directories(output_dir_);
    
    // Initialize CSV output
    if (output_csv_) {
        std::string csv_path = output_dir_ + "/" + run_name_ + ".csv";
        csv_file_ = std::make_unique<std::ofstream>(csv_path);
        if (csv_file_->is_open()) {
            // Write CSV header with UTC information
            *csv_file_ << "tick,simulation_time,utc_time,position_x,position_y,position_z,"
                      << "velocity_x,velocity_y,velocity_z,"
                      << "orientation_x,orientation_y,orientation_z,orientation_w,"
                      << "atm_density,atm_pressure,atm_temperature,"
                      << "gravity_x,gravity_y,gravity_z,"
                      << "wind_speed_x,wind_speed_y,wind_speed_z\n";
            LOG_INFO("CSV output initialized: " + csv_path, "OutputManager");
            LOG_INFO("Simulation start UTC: " + time_manager.getStartUTCString(), "OutputManager");
        } else {
            LOG_ERROR("Failed to create CSV file: " + csv_path, "OutputManager");
        }
    }
    
    // Initialize JSON output
    if (output_json_) {
        std::string json_path = output_dir_ + "/" + run_name_ + ".json";
        json_file_ = std::make_unique<std::ofstream>(json_path);
        if (json_file_->is_open()) {
            *json_file_ << "{\n";
            *json_file_ << "  \"metadata\": {\n";
            *json_file_ << "    \"run_name\": \"" << run_name_ << "\",\n";
            *json_file_ << "    \"start_utc\": " << std::fixed << std::setprecision(3) << time_manager.getStartUTC() << ",\n";
            *json_file_ << "    \"start_utc_string\": \"" << time_manager.getStartUTCString() << "\",\n";
            *json_file_ << "    \"output_interval\": " << output_interval_ << "\n";
            *json_file_ << "  },\n  \"data\": [\n";
            LOG_INFO("JSON output initialized: " + json_path, "OutputManager");
        } else {
            LOG_ERROR("Failed to create JSON file: " + json_path, "OutputManager");
        }
    }
    
    // Clear previous data
    data_points_.clear();
    metrics_.clear();
    last_recorded_tick_ = -1;
    initialized_ = true;
    
    LOG_INFO("OutputManager initialized for run: " + run_name_, "OutputManager");
}

void OutputManager::recordState(const std::vector<uint8_t>& state_buffer, double simulation_time, double utc_time, int tick) {
    if (tick % output_interval_ != 0) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!initialized_) {
        LOG_WARNING("OutputManager not initialized, skipping state recording", "OutputManager");
        return;
    }
    
    const state_vector::GeneralState* state = state_vector::GetGeneralState(state_buffer.data());
    if (!state) {
        LOG_ERROR("Failed to parse state buffer for output", "OutputManager");
        return;
    }
    
    SimulationDataPoint point = extractDataPoint(state, simulation_time, utc_time, tick);
    data_points_.push_back(point);
    
    if (output_csv_ && csv_file_ && csv_file_->is_open()) {
        writeDataPointCSV(point, tick);
    }
    
    if (output_json_ && json_file_ && json_file_->is_open()) {
        writeDataPointJSON(point, tick);
    }
    
    last_recorded_tick_ = tick;
}

void OutputManager::recordMetrics(const std::string& component, const std::string& metric_name, double value) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    metrics_[component][metric_name].push_back(value);
}

void OutputManager::finalizeOutput() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (!initialized_) {
        return;
    }
    
    // Close CSV file
    if (csv_file_ && csv_file_->is_open()) {
        csv_file_->close();
        LOG_INFO("CSV output finalized", "OutputManager");
    }
    
    // Close JSON file
    if (json_file_ && json_file_->is_open()) {
        *json_file_ << "\n  ],\n";
        writeMetricsJSON();
        *json_file_ << "}\n";
        json_file_->close();
        LOG_INFO("JSON output finalized", "OutputManager");
    }
    
    initialized_ = false;
    LOG_INFO("OutputManager finalized. Recorded " + std::to_string(data_points_.size()) + " data points", "OutputManager");
}

void OutputManager::flush() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (csv_file_ && csv_file_->is_open()) {
        csv_file_->flush();
    }
    
    if (json_file_ && json_file_->is_open()) {
        json_file_->flush();
    }
}

void OutputManager::printSummary() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (data_points_.empty()) {
        LOG_INFO("No simulation data recorded", "OutputManager");
        return;
    }
    
    const auto& first = data_points_.front();
    const auto& last = data_points_.back();
    
    LOG_INFO("=== SIMULATION RESULTS SUMMARY ===", "OutputManager");
    LOG_INFO("Total data points: " + std::to_string(data_points_.size()), "OutputManager");
    LOG_INFO("Simulation time: " + std::to_string(first.time) + "s to " + std::to_string(last.time) + "s", "OutputManager");
    
    LOG_INFO("Initial position: (" + 
             std::to_string(first.position_x) + ", " + 
             std::to_string(first.position_y) + ", " + 
             std::to_string(first.position_z) + ")", "OutputManager");
             
    LOG_INFO("Final position: (" + 
             std::to_string(last.position_x) + ", " + 
             std::to_string(last.position_y) + ", " + 
             std::to_string(last.position_z) + ")", "OutputManager");
             
    LOG_INFO("Initial velocity: (" + 
             std::to_string(first.velocity_x) + ", " + 
             std::to_string(first.velocity_y) + ", " + 
             std::to_string(first.velocity_z) + ")", "OutputManager");
             
    LOG_INFO("Final velocity: (" + 
             std::to_string(last.velocity_x) + ", " + 
             std::to_string(last.velocity_y) + ", " + 
             std::to_string(last.velocity_z) + ")", "OutputManager");
             
    // Calculate distance traveled
    double distance = sqrt(
        pow(last.position_x - first.position_x, 2) + 
        pow(last.position_y - first.position_y, 2) + 
        pow(last.position_z - first.position_z, 2)
    );
    LOG_INFO("Distance traveled: " + std::to_string(distance) + " units", "OutputManager");
    
    LOG_INFO("Output files saved in: " + output_dir_, "OutputManager");
}

SimulationDataPoint OutputManager::extractDataPoint(const state_vector::GeneralState* state, double time, double utc_time, int tick) {
    SimulationDataPoint point;
    
    point.time = time;
    point.utc_time = utc_time;
    
    // Position
    if (state->position()) {
        point.position_x = state->position()->x();
        point.position_y = state->position()->y();
        point.position_z = state->position()->z();
    }
    
    // Velocity
    if (state->velocity()) {
        point.velocity_x = state->velocity()->x();
        point.velocity_y = state->velocity()->y();
        point.velocity_z = state->velocity()->z();
    }
    
    // Orientation (quaternion)
    if (state->orientation()) {
        point.orientation_x = state->orientation()->x();
        point.orientation_y = state->orientation()->y();
        point.orientation_z = state->orientation()->z();
        point.orientation_w = state->orientation()->w();
    }
    
    // Atmospheric conditions
    point.atm_density = state->atm_density();
    point.atm_pressure = state->atm_pressure();
    point.atm_temperature = state->atm_temperature();
    
    // Gravity
    if (state->gravity()) {
        point.gravity_x = state->gravity()->x();
        point.gravity_y = state->gravity()->y();
        point.gravity_z = state->gravity()->z();
    }
    
    // Wind speed
    if (state->wind_speed()) {
        point.wind_speed_x = state->wind_speed()->x();
        point.wind_speed_y = state->wind_speed()->y();
        point.wind_speed_z = state->wind_speed()->z();
    }
    
    return point;
}

void OutputManager::writeCSVHeader() {
    if (!csv_file_ || !csv_file_->is_open()) return;
    
    *csv_file_ << "tick,simulation_time,utc_time,position_x,position_y,position_z,"
              << "velocity_x,velocity_y,velocity_z,"
              << "orientation_x,orientation_y,orientation_z,orientation_w,"
              << "atm_density,atm_pressure,atm_temp,"
              << "gravity_x,gravity_y,gravity_z,"
              << "wind_x,wind_y,wind_z\n";
}

void OutputManager::writeDataPointCSV(const SimulationDataPoint& point, int tick) {
    if (!csv_file_ || !csv_file_->is_open()) return;
    
    *csv_file_ << tick << "," << std::fixed << std::setprecision(6)
               << point.time << "," << std::fixed << std::setprecision(6) << point.utc_time << ","
               << point.position_x << "," << point.position_y << "," << point.position_z << ","
               << point.velocity_x << "," << point.velocity_y << "," << point.velocity_z << ","
               << point.orientation_x << "," << point.orientation_y << "," << point.orientation_z << "," << point.orientation_w << ","
               << point.atm_density << "," << point.atm_pressure << "," << point.atm_temperature << ","
               << point.gravity_x << "," << point.gravity_y << "," << point.gravity_z << ","
               << point.wind_speed_x << "," << point.wind_speed_y << "," << point.wind_speed_z << "\n";
}

void OutputManager::writeDataPointJSON(const SimulationDataPoint& point, int tick) {
    if (!json_file_ || !json_file_->is_open()) return;
    
    if (last_recorded_tick_ >= 0) {
        *json_file_ << ",\n";
    }
    
    *json_file_ << "    {\n";
    *json_file_ << "      \"tick\": " << tick << ",\n";
    *json_file_ << "      \"simulation_time\": " << std::fixed << std::setprecision(6) << point.time << ",\n";
    *json_file_ << "      \"utc_time\": " << std::fixed << std::setprecision(6) << point.utc_time << ",\n";
    *json_file_ << "      \"position\": [" << point.position_x << ", " << point.position_y << ", " << point.position_z << "],\n";
    *json_file_ << "      \"velocity\": [" << point.velocity_x << ", " << point.velocity_y << ", " << point.velocity_z << "],\n";
    *json_file_ << "      \"orientation\": [" << point.orientation_x << ", " << point.orientation_y << ", " << point.orientation_z << ", " << point.orientation_w << "],\n";
    *json_file_ << "      \"atmosphere\": {\"density\": " << point.atm_density << ", \"pressure\": " << point.atm_pressure << ", \"temperature\": " << point.atm_temperature << "},\n";
    *json_file_ << "      \"gravity\": [" << point.gravity_x << ", " << point.gravity_y << ", " << point.gravity_z << "],\n";
    *json_file_ << "      \"wind\": [" << point.wind_speed_x << ", " << point.wind_speed_y << ", " << point.wind_speed_z << "]\n";
    *json_file_ << "    }";
}

void OutputManager::writeMetricsJSON() {
    if (!json_file_ || !json_file_->is_open()) return;
    
    *json_file_ << "  \"metrics\": {\n";
    
    bool first_component = true;
    for (const auto& component_pair : metrics_) {
        if (!first_component) *json_file_ << ",\n";
        first_component = false;
        
        *json_file_ << "    \"" << component_pair.first << "\": {\n";
        
        bool first_metric = true;
        for (const auto& metric_pair : component_pair.second) {
            if (!first_metric) *json_file_ << ",\n";
            first_metric = false;
            
            *json_file_ << "      \"" << metric_pair.first << "\": [";
            
            bool first_value = true;
            for (double value : metric_pair.second) {
                if (!first_value) *json_file_ << ", ";
                first_value = false;
                *json_file_ << value;
            }
            
            *json_file_ << "]";
        }
        
        *json_file_ << "\n    }";
    }
    
    *json_file_ << "\n  }\n";
}
