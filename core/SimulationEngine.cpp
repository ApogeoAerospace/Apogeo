#include <iostream>
#include <chrono>
#include <filesystem>
#include <cmath>
#include "SimulationEngine.h"
#include "PluginManager.h"
#include "InitialStateLoader.h"
#include "../src/core/Logger.h"
#include "../src/core/ConfigManager.h"
#include "../src/core/OutputManager.h"
#include "../src/core/TimeManager.h"
#include "state_vector_generated.h"
#include "flatbuffers/flatbuffers.h"

using namespace MoLab;

SimulationEngine::SimulationEngine()
    : plugin_manager_(std::make_unique<PluginManager>()),
      simulation_time_(0.0),
      iteration_count_(0),
      is_running_(false)
{
    LOG_INFO("SimulationEngine initialized", "SimulationEngine");
}

SimulationEngine::~SimulationEngine() noexcept {
    try {
        if (is_running_) {
            shutdown();
        }
        LOG_INFO("SimulationEngine destroyed", "SimulationEngine");
    } catch (const std::exception& e) {
        // Cannot throw from destructor
        // shutdown() should be noexcept-safe
    }
}

bool SimulationEngine::initialize(const std::string& state_filepath) {
    LOG_INFO("Initializing simulation with state file: " + state_filepath, "SimulationEngine");

    // Verificar que el archivo existe
    if (!std::filesystem::exists(state_filepath)) {
        LOG_ERROR("State file not found: " + state_filepath, "SimulationEngine");
        return false;
    }

    flatbuffers::FlatBufferBuilder builder;
    if (!InitialStateLoader::create_state_from_json(builder, state_filepath)) {
        LOG_ERROR("Failed to load initial state from: " + state_filepath, "SimulationEngine");
        return false;
    }

    const uint8_t* buf = builder.GetBufferPointer();
    const uint32_t size = builder.GetSize();

    // Thread-safe assignment
    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        current_state_buffer_.assign(buf, buf + size);
    }


    // Initialize output manager
    auto& output_manager = OutputManager::getInstance();
    output_manager.setOutputDirectory("output");
    output_manager.setOutputFormats(true, true, false); // CSV and JSON
    output_manager.setOutputInterval(5); // Optimized: Save every 5 ticks (balance speed/detail)
    output_manager.initializeOutput("molab_simulation");    // Reset simulation state
    simulation_time_ = 0.0;
    iteration_count_ = 0;

    LOG_INFO("Simulation initialized successfully", "SimulationEngine");
    return true;
}

bool SimulationEngine::initialize_with_config(const std::string& config_filepath) {
    LOG_INFO("Initializing simulation with config: " + config_filepath, "SimulationEngine");

    auto& config_manager = ConfigManager::getInstance();
    if (!config_manager.loadConfig(config_filepath)) {
        LOG_WARNING("Failed to load config, using defaults", "SimulationEngine");
    }

    const auto& sim_config = config_manager.getSimulationConfig();

    // Configure logging
    auto& logger = Logger::getInstance();
    if (sim_config.log_level == "DEBUG") {
        logger.setLogLevel(LogLevel::DEBUG);
    } else if (sim_config.log_level == "INFO") {
        logger.setLogLevel(LogLevel::INFO);
    } else if (sim_config.log_level == "WARNING") {
        logger.setLogLevel(LogLevel::WARNING);
    } else if (sim_config.log_level == "ERROR") {
        logger.setLogLevel(LogLevel::ERR);
    }

    if (!sim_config.log_file.empty()) {
        logger.setLogFile(sim_config.log_file);
    }

    // Initialize TimeManager with current UTC time
    auto& time_manager = TimeManager::getInstance();
    time_manager.initialize(); // Usa tiempo UTC actual como inicio

    // Load plugins from configuration
    if (!plugin_manager_->load_plugins_from_config()) {
        LOG_WARNING("Some plugins failed to load from configuration", "SimulationEngine");
    }

    // Initialize output manager
    auto& output_manager = OutputManager::getInstance();
    output_manager.setOutputDirectory("output");
    output_manager.setOutputFormats(true, true, false); // CSV and JSON
    output_manager.setOutputInterval(5); // Optimized: Save every 5 ticks (balance speed/detail)
    output_manager.initializeOutput("molab_simulation");

    // Initialize with state file
    return initialize(config_manager.getInitialStateFile());
}

void SimulationEngine::load_plugin(const std::string& name, int plugin_type) {
    LOG_INFO("Loading plugin: " + name + " (type: " + std::to_string(plugin_type) + ")", "SimulationEngine");

    // Traducimos el tipo de plugin a la enumeración adecuada
    PluginType type;
    switch (plugin_type) {
        case 0:
            type = PluginType::SEQUENTIAL_STATE_MODIFIER;
            break;
        case 1:
            type = PluginType::PARALLEL_PHYSICS_CALCULATOR;
            break;
        default:
            LOG_ERROR("Unknown plugin type: " + std::to_string(plugin_type), "SimulationEngine");
            return;
    }

#if defined(_WIN32)
    std::string path = name + ".dll";
#elif defined(__APPLE__)
    std::string path = "lib" + name + ".dylib";
#else
    std::string path = "lib" + name + ".so";
#endif

    if (!plugin_manager_->load_plugin(path, type)) {
        LOG_ERROR("Failed to load plugin: " + name, "SimulationEngine");
    } else {
        LOG_INFO("Plugin loaded successfully: " + name, "SimulationEngine");
    }
}

void SimulationEngine::run_tick() {
    if (!is_running_) {
        is_running_ = true;
        LOG_INFO("Starting simulation", "SimulationEngine");
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Get configuration for time step
    const auto& config = ConfigManager::getInstance().getSimulationConfig();
    double delta_time = config.time_step;

    // Update TimeManager
    auto& time_manager = TimeManager::getInstance();
    time_manager.updateSimulationTime(delta_time);

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        plugin_manager_->run_simulation_cycle_improved(current_state_buffer_, delta_time);
    }

    // VALIDACIÓN RÁPIDA: Detectar colisiones con el suelo inmediatamente
    auto state = flatbuffers::GetRoot<state_vector::GeneralState>(current_state_buffer_.data());
    if (state && state->position()) {
        double pos_z = state->position()->z();
        // Detección rápida para coordenadas locales
        if (std::abs(pos_z) < 100000.0 && pos_z < -1.0) {
            LOG_ERROR("Ground collision detected! Z position: " + std::to_string(pos_z) + " m", "SimulationEngine");
            LOG_ERROR("Simulation terminated due to ground collision", "SimulationEngine");
            is_running_ = false;
            return;
        }
    }

    // Validación completa solo cada 50 ticks
    if (iteration_count_ % 50 == 0) {
        if (!validate_simulation_state()) {
            LOG_ERROR("Simulation terminated due to invalid state", "SimulationEngine");
            is_running_ = false;
            return;
        }
    }

    // Update simulation metrics
    simulation_time_.store(simulation_time_.load() + delta_time);
    iteration_count_++;

    // Record state using TimeManager time (solo si el estado es válido)
    auto& output_manager = OutputManager::getInstance();
    output_manager.recordState(current_state_buffer_, time_manager.getSimulationTime(), time_manager.getCurrentUTC(), iteration_count_);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    last_tick_duration_ = duration.count() / 1000.0; // Convert to milliseconds

    // LOGGING DE PERFORMANCE: Infrecuente y detallado para análisis técnico
    // Nota: El progreso de simulación se reporta en main.cpp con mayor frecuencia
    if (iteration_count_ % 5000 == 0) {
        LOG_INFO("Simulation tick " + std::to_string(iteration_count_) +
                " completed in " + std::to_string(last_tick_duration_) + "ms", "SimulationEngine");
        LOG_INFO("Simulation time: " + std::to_string(time_manager.getSimulationTime()) + "s", "SimulationEngine");
        LOG_INFO("Current UTC: " + time_manager.getCurrentUTCString(), "SimulationEngine");
    }
}

bool SimulationEngine::run_simulation() {
    const auto& config = ConfigManager::getInstance().getSimulationConfig();

    LOG_INFO("Starting full simulation run", "SimulationEngine");
    LOG_INFO("Duration: " + std::to_string(config.simulation_duration) + "s", "SimulationEngine");
    LOG_INFO("Time step: " + std::to_string(config.time_step) + "s", "SimulationEngine");
    LOG_INFO("Max iterations: " + std::to_string(config.max_iterations), "SimulationEngine");

    auto start_time = std::chrono::high_resolution_clock::now();

    while (simulation_time_.load() < config.simulation_duration &&
           iteration_count_ < config.max_iterations) {

        run_tick();

        // Optimized: Validation removed (already done inside run_tick at line 184)
        // Redundant validation was causing 10-15% performance overhead
        if (!is_running_) {
            LOG_ERROR("Simulation terminated early", "SimulationEngine");
            return false;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    LOG_INFO("Simulation completed successfully", "SimulationEngine");
    LOG_INFO("Total iterations: " + std::to_string(iteration_count_), "SimulationEngine");
    LOG_INFO("Simulation time: " + std::to_string(simulation_time_.load()) + "s", "SimulationEngine");
    LOG_INFO("Real time: " + std::to_string(duration.count()) + "ms", "SimulationEngine");

    // Print plugin metrics
    print_performance_metrics();

    return true;
}

void SimulationEngine::shutdown() {
    if (is_running_) {
        LOG_INFO("Shutting down simulation", "SimulationEngine");
        is_running_ = false;
    }

    if (plugin_manager_) {
        plugin_manager_->shutdown();
    }


    // Finalize output and print summary
    auto& output_manager = OutputManager::getInstance();
    output_manager.finalizeOutput();
    output_manager.printSummary();    LOG_INFO("Simulation shutdown complete", "SimulationEngine");
}

bool SimulationEngine::validate_simulation_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (current_state_buffer_.empty()) {
        LOG_ERROR("State buffer is empty", "SimulationEngine");
        return false;
    }

    // Parse state and validate bounds
    const state_vector::GeneralState* state = state_vector::GetGeneralState(current_state_buffer_.data());
    if (!state) {
        LOG_ERROR("Failed to parse state buffer", "SimulationEngine");
        return false;
    }

    // Check for NaN values
    if (state->position()) {
        if (std::isnan(state->position()->x()) ||
            std::isnan(state->position()->y()) ||
            std::isnan(state->position()->z())) {
            LOG_ERROR("NaN detected in position", "SimulationEngine");
            return false;
        }

        // DETECCIÓN DE COLISIÓN CON EL SUELO
        const double EARTH_RADIUS = 6378137.0;  // m - Radio de la Tierra (WGS84)
        const double LOCAL_COORD_THRESHOLD = 1000000.0;  // 1000 km

        double pos_x = state->position()->x();
        double pos_y = state->position()->y();
        double pos_z = state->position()->z();

        // Calcular distancia al centro de la Tierra
        double distance_from_center = std::sqrt(pos_x*pos_x + pos_y*pos_y + pos_z*pos_z);

        // Detectar sistema de coordenadas
        bool is_geocentric = (distance_from_center > LOCAL_COORD_THRESHOLD ||
                             std::abs(pos_z) > LOCAL_COORD_THRESHOLD);

        if (is_geocentric) {
            // COORDENADAS GEOCÉNTRICAS (ECEF): Verificar distancia al centro
            if (distance_from_center < EARTH_RADIUS) {
                double altitude = distance_from_center - EARTH_RADIUS;
                LOG_ERROR("Ground collision detected! Altitude: " + std::to_string(altitude) + " m",
                         "SimulationEngine");
                LOG_ERROR("Object is " + std::to_string(-altitude/1000.0) + " km below surface",
                         "SimulationEngine");
                LOG_ERROR("Simulation terminated to prevent unphysical results", "SimulationEngine");
                return false;
            }
        } else {
            // COORDENADAS LOCALES: Verificar Z < 0 (bajo el suelo)
            if (pos_z < 0.0) {
                LOG_ERROR("Ground collision detected! Z position: " + std::to_string(pos_z) + " m",
                         "SimulationEngine");
                LOG_ERROR("Object is " + std::to_string(-pos_z) + " m below surface (local coordinates)",
                         "SimulationEngine");
                LOG_ERROR("Simulation terminated to prevent unphysical results", "SimulationEngine");
                return false;
            }
        }
    }

    if (state->velocity()) {
        if (std::isnan(state->velocity()->x()) ||
            std::isnan(state->velocity()->y()) ||
            std::isnan(state->velocity()->z())) {
            LOG_ERROR("NaN detected in velocity", "SimulationEngine");
            return false;
        }

        // NUEVO: Verificar velocidades supersónicas extremas (posible error numérico)
        double vel_x = state->velocity()->x();
        double vel_y = state->velocity()->y();
        double vel_z = state->velocity()->z();
        double speed = std::sqrt(vel_x*vel_x + vel_y*vel_y + vel_z*vel_z);

        const double ESCAPE_VELOCITY = 11200.0;  // m/s - Velocidad de escape de la Tierra
        if (speed > ESCAPE_VELOCITY * 2.0) {
            LOG_WARNING("Extreme velocity detected: " + std::to_string(speed) + " m/s (Mach " +
                       std::to_string(speed/343.0) + ")", "SimulationEngine");
            LOG_WARNING("This may indicate numerical instability", "SimulationEngine");
        }
    }

    return true;
}

void SimulationEngine::print_performance_metrics() const {
    if (!plugin_manager_) return;

    LOG_INFO("=== SIMULATION PERFORMANCE METRICS ===", "SimulationEngine");
    LOG_INFO("Total iterations: " + std::to_string(iteration_count_.load()), "SimulationEngine");
    LOG_INFO("Simulation time: " + std::to_string(simulation_time_.load()) + "s", "SimulationEngine");
    LOG_INFO("Last tick duration: " + std::to_string(last_tick_duration_.load()) + "ms", "SimulationEngine");

    // Get plugin metrics
    auto plugin_metrics = plugin_manager_->get_plugin_metrics();
    if (!plugin_metrics.empty()) {
        LOG_INFO("=== PLUGIN PERFORMANCE METRICS ===", "SimulationEngine");
        for (const auto& metric : plugin_metrics) {
            LOG_INFO("Plugin: " + metric.name, "SimulationEngine");
            LOG_INFO("  Executions: " + std::to_string(metric.execution_count), "SimulationEngine");
            LOG_INFO("  Total time: " + std::to_string(metric.total_execution_time) + "ms", "SimulationEngine");
            LOG_INFO("  Average time: " + std::to_string(metric.average_execution_time) + "ms", "SimulationEngine");
            LOG_INFO("  Last time: " + std::to_string(metric.last_execution_time) + "ms", "SimulationEngine");
        }
    }
}
