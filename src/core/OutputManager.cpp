#include "OutputManager.h"
#include "Logger.h"
#include "TimeManager.h"
#include "ConfigManager.h"
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

    // OPTIMIZACIÓN CRÍTICA: Pre-allocate vector capacity to avoid reallocations
    // Eliminates cuadrática degradation in performance
    data_points_.clear();

    // Calculate expected number of data points
    // This prevents expensive vector reallocations during simulation
    try {
        auto& config = ConfigManager::getInstance().getSimulationConfig();
        double simulation_duration = config.simulation_duration;
        double time_step = config.time_step;

        if (time_step > 0 && simulation_duration > 0) {
            int total_ticks = static_cast<int>(simulation_duration / time_step);
            int expected_points = (total_ticks / output_interval_) + 100; // +100 safety margin

            data_points_.reserve(expected_points);

            LOG_INFO("Pre-allocated capacity for " + std::to_string(expected_points) +
                    " data points (eliminates vector reallocations)", "OutputManager");
        }
    } catch (...) {
        // If config not available, use reasonable default
        data_points_.reserve(2000); // Default capacity for typical simulations
        LOG_WARNING("Using default vector capacity (2000 points)", "OutputManager");
    }

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

    // OPTIMIZACIÓN: Constantes físicas pre-calculadas (static = calculadas una sola vez)
    static const double EARTH_RADIUS = 6378137.0;  // m (WGS84 equatorial radius)
    static const double EARTH_MASS = 5.972e24;     // kg
    static const double GRAVITATIONAL_CONSTANT = 6.674e-11; // m³/(kg·s²)
    static const double STANDARD_GRAVITY = 9.80665; // m/s² at sea level
    static const double GM = GRAVITATIONAL_CONSTANT * EARTH_MASS; // Pre-calculated
    static const double R_EARTH_SQ = EARTH_RADIUS * EARTH_RADIUS; // Pre-calculated

    // Calcular distancia al centro de la Tierra
    double distance_from_center = std::sqrt(
        point.position_x * point.position_x +
        point.position_y * point.position_y +
        point.position_z * point.position_z
    );

    // Si la posición está cerca del origen (sistema local), asumir superficie
    if (distance_from_center < 100000) { // menos de 100 km - coordenadas locales
        // Asume que Z es altitud en sistema local
        distance_from_center = EARTH_RADIUS + std::abs(point.position_z);
    }

    // Calcular altitud sobre el nivel del mar
    double altitude = distance_from_center - EARTH_RADIUS;
    if (altitude < 0) altitude = 0;

    // OPTIMIZACIÓN: Caché de valores atmosféricos
    // Solo recalcular ISA si altitud cambió significativamente (>100m)
    static double cached_altitude = -1000.0;  // Valor imposible para forzar primer cálculo
    static double cached_temperature = 288.15;
    static double cached_pressure = 101325.0;
    static double cached_density = 1.225;

    // MODELO ATMOSFÉRICO REALISTA (International Standard Atmosphere)
    double temperature, pressure, density;

    // Verificar si necesitamos recalcular (tolerancia: 100m)
    if (std::abs(altitude - cached_altitude) > 100.0) {
        // Recalcular ISA completo
        if (altitude < 11000) { // Troposfera (0-11 km)
            temperature = 288.15 - 0.0065 * altitude;
            pressure = 101325.0 * std::pow(temperature / 288.15, 5.256);
            density = pressure / (287.0 * temperature);
        } else if (altitude < 20000) { // Tropopausa (11-20 km)
            temperature = 216.65;
            pressure = 22632.1 * std::exp(-0.0001577 * (altitude - 11000));
            density = pressure / (287.0 * temperature);
        } else if (altitude < 32000) { // Estratosfera baja (20-32 km)
            temperature = 216.65 + 0.001 * (altitude - 20000);
            pressure = 5474.89 * std::pow(temperature / 216.65, -34.164);
            density = pressure / (287.0 * temperature);
        } else { // Más allá de 32 km - modelo exponencial simple
            temperature = 228.65 + 0.0028 * (altitude - 32000);
            if (temperature > 270) temperature = 270;
            // Modelo exponencial para altitudes extremas
            double scale_height = 8400.0; // m
            density = 1.225 * std::exp(-altitude / scale_height);
            pressure = density * 287.0 * temperature;
        }

        // Asegurar valores mínimos para altitudes extremas
        if (altitude > 100000) { // Más de 100 km
            density = 1.225 * std::exp(-altitude / 8400.0);
            if (density < 1e-10) density = 1e-10;
            pressure = density * 287.0 * temperature;
        }

        // Actualizar caché con nuevos valores
        cached_altitude = altitude;
        cached_temperature = temperature;
        cached_pressure = pressure;
        cached_density = density;
    } else {
        // Usar valores del caché (ahorro de ~70% en cálculos atmosféricos)
        temperature = cached_temperature;
        pressure = cached_pressure;
        density = cached_density;
    }

    point.atm_density = density;
    point.atm_pressure = pressure;
    point.atm_temperature = temperature;

    // GRAVEDAD REALISTA - Ley del cuadrado inverso (optimizado con GM pre-calculado)
    double gravity_magnitude;

    // Detectar sistema de coordenadas basado en posición original (antes de modificar distance_from_center)
    double original_distance = std::sqrt(
        point.position_x * point.position_x +
        point.position_y * point.position_y +
        point.position_z * point.position_z
    );

    if (original_distance > 100000.0) {
        // Sistema GEOCÉNTRICO (coordenadas ECEF)
        // Usar distancia real al centro de la Tierra
        double distance_sq = distance_from_center * distance_from_center;
        gravity_magnitude = GM / distance_sq;

        // Dirección: hacia el centro de la Tierra
        if (distance_from_center > 0) {
            double inv_norm = 1.0 / distance_from_center;
            point.gravity_x = -gravity_magnitude * point.position_x * inv_norm;
            point.gravity_y = -gravity_magnitude * point.position_y * inv_norm;
            point.gravity_z = -gravity_magnitude * point.position_z * inv_norm;
        } else {
            point.gravity_x = 0.0;
            point.gravity_y = 0.0;
            point.gravity_z = -STANDARD_GRAVITY;
        }
    } else {
        // Sistema LOCAL (coordenadas topocéntricas)
        // Gravedad simplificada: g(h) = g0 * (R/(R+h))²
        double r_ratio = EARTH_RADIUS / distance_from_center;
        gravity_magnitude = STANDARD_GRAVITY * r_ratio * r_ratio;

        // Dirección: hacia abajo (-Z en local)
        point.gravity_x = 0.0;
        point.gravity_y = 0.0;
        point.gravity_z = -gravity_magnitude;
    }

    // Wind speed - mantener los valores del estado o calcular modelo simple
    if (state->wind_speed()) {
        // Modelo de viento que varía con altitud
        double wind_factor = 1.0;
        if (altitude > 8000 && altitude < 15000) {
            // Jet stream entre 8-15 km
            wind_factor = 5.0 + 10.0 * std::sin(3.14159 * (altitude - 8000) / 7000);
        }
        point.wind_speed_x = state->wind_speed()->x() * wind_factor;
        point.wind_speed_y = state->wind_speed()->y() * wind_factor;
        point.wind_speed_z = state->wind_speed()->z();
    } else {
        point.wind_speed_x = 0.0;
        point.wind_speed_y = 0.0;
        point.wind_speed_z = 0.0;
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
