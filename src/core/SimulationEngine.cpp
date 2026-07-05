#include <iostream>
#include <chrono>
#include <filesystem>
#include <cmath>
#include "SimulationEngine.h"
#include "PluginManager.h"
#include "InitialStateLoader.h"
#include "Logger.h"
#include "ConfigManager.h"
#include "OutputManager.h"
#include "TimeManager.h"
#include "../FlightComputerContext.h"
#include "state_vector_generated.h"
#include "flatbuffers/flatbuffers.h"

/**
 * @file SimulationEngine.cpp
 * @brief Implementation of the main simulation engine.
 */

namespace MoLab {

SimulationEngine::SimulationEngine()
    : plugin_manager_(std::make_unique<MoLab::PluginManager>()),
      simulation_time_(0.0),
      iteration_count_(0),
      is_running_(false)
{
    LOG_INFO("SimulationEngine initialized", "SimulationEngine");
}

SimulationEngine::~SimulationEngine() noexcept {
    try {
        shutdown();
        LOG_INFO("SimulationEngine destroyed", "SimulationEngine");
    } catch (const std::exception&) {
        // Cannot throw from destructor
    }
}

bool SimulationEngine::initialize(const std::string& state_filepath) {
    LOG_INFO("Initializing simulation with state file: " + state_filepath, "SimulationEngine");

    // Initialize TimeManager with current UTC time
    auto& time_manager = TimeManager::getInstance();
    time_manager.initialize(); // Use current UTC time as start

    // Verify that the file exists
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

    const auto& config_mgr = ConfigManager::getInstance();
    if (!config_mgr.flight_script_path.empty()) {
        flight_computer_ = std::make_unique<FlightComputerContext>(config_mgr.flight_script_path);
    }


    // Initialize output manager
    auto& output_manager = OutputManager::getInstance();
    output_manager.setOutputDirectory("output");
    output_manager.setOutputFormats(true, true, false); // CSV and JSON
    output_manager.setOutputInterval(5); // Optimized: Save every 5 ticks (balance speed/detail)
    output_manager.initializeOutput("molab_simulation");    // Reset simulation state
    simulation_time_ = 0.0;
    iteration_count_ = 0;
    last_tick_duration_ = 0.0;
    compute_tick_duration_ms_ = 0.0;
    io_tick_duration_ms_ = 0.0;

    LOG_INFO("Simulation initialized successfully", "SimulationEngine");
    return true;
}

bool SimulationEngine::initialize_from_loaded_config() {
    LOG_INFO("Initializing simulation", "SimulationEngine");

    // Load plugins from configuration
    if (!plugin_manager_->load_plugins_from_config()) {
        LOG_WARNING("Some plugins failed to load from configuration", "SimulationEngine");
    }

    // Initialize with state file
    return initialize(ConfigManager::getInstance().getInitialStateFile());
}

void SimulationEngine::load_plugin(const std::string& name, int plugin_type) {
    LOG_INFO("Loading plugin: " + name + " (type: " + std::to_string(plugin_type) + ")", "SimulationEngine");

    // Translate plugin type to appropriate enumeration
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

#ifdef _WIN32
    std::string path = name + ".dll";
#elif __APPLE__
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

  auto compute_start_time = std::chrono::high_resolution_clock::now();

  double delta_time = 0.0;
  const state_vector::GeneralState* state = nullptr;

  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto state_before = flatbuffers::GetRoot<state_vector::GeneralState>(current_state_buffer_.data());
    if (state_before) {
      delta_time = static_cast<double>(state_before->dt());
    }

    if (delta_time > 0.0) {
      if (flight_computer_ && flight_computer_->is_valid()) {
          auto* state_ptr = state_vector::GetGeneralState(current_state_buffer_.data());
          flatbuffers::FlatBufferBuilder fbb;
          flight_computer_->update(state_ptr, fbb, static_cast<float>(delta_time));
          const uint8_t* new_buf = fbb.GetBufferPointer();
          uint32_t new_sz = fbb.GetSize();
          if (new_sz > 0) {
              current_state_buffer_.assign(new_buf, new_buf + new_sz);
          }
      }
      plugin_manager_->run_simulation_cycle(current_state_buffer_, delta_time);
      state = flatbuffers::GetRoot<state_vector::GeneralState>(current_state_buffer_.data());
    }
  }

  if (delta_time <= 0.0) {
    LOG_ERROR("Invalid dt in state buffer (<= 0). Aborting tick.", "SimulationEngine");
    is_running_ = false;
    return;
  }

  if (!state) {
    LOG_ERROR("Failed to parse state buffer after tick", "SimulationEngine");
    is_running_ = false;
    return;
  }

  auto& time_manager = TimeManager::getInstance();
  time_manager.updateSimulationTime(delta_time);

  if (iteration_count_ % 50 == 0) {
    if (!validate_simulation_state(state)) {
      LOG_ERROR("Simulation terminated due to invalid state", "SimulationEngine");
      is_running_ = false;
      return;
    }
  }

  simulation_time_.store(simulation_time_.load() + delta_time);
  iteration_count_++;

  auto compute_end_time = std::chrono::high_resolution_clock::now();

  auto& output_manager = OutputManager::getInstance();
  auto io_start_time = compute_end_time;
  output_manager.recordState(state, time_manager.getSimulationTime(), time_manager.getCurrentUTC(), iteration_count_);
  auto io_end_time = std::chrono::high_resolution_clock::now();

  auto compute_duration = std::chrono::duration_cast<std::chrono::microseconds>(compute_end_time - compute_start_time);
  auto io_duration = std::chrono::duration_cast<std::chrono::microseconds>(io_end_time - io_start_time);
  compute_tick_duration_ms_ = compute_duration.count() / 1000.0;
  io_tick_duration_ms_ = io_duration.count() / 1000.0;
  last_tick_duration_ = compute_tick_duration_ms_.load() + io_tick_duration_ms_.load();

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
  LOG_INFO("Max iterations: " + std::to_string(config.max_iterations), "SimulationEngine");

  auto start_time = std::chrono::high_resolution_clock::now();

  while (simulation_time_.load() < config.simulation_duration &&
         iteration_count_ < config.max_iterations) {

    run_tick();

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

SimulationEngine::EngineStatus SimulationEngine::getStatus() const {
    return EngineStatus{
        is_running_.load(),
        iteration_count_.load(),
        simulation_time_.load(),
        last_tick_duration_.load(),
        compute_tick_duration_ms_.load(),
        io_tick_duration_ms_.load()
    };
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
    output_manager.printSummary();

    LOG_INFO("Simulation shutdown complete", "SimulationEngine");
}

bool SimulationEngine::validate_simulation_state() const {
    std::lock_guard<std::mutex> lock(state_mutex_);

    if (current_state_buffer_.empty()) {
        LOG_ERROR("State buffer is empty", "SimulationEngine");
        return false;
    }

    const state_vector::GeneralState* state = state_vector::GetGeneralState(current_state_buffer_.data());
    if (!state) {
        LOG_ERROR("Failed to parse state buffer", "SimulationEngine");
        return false;
    }

    return validate_simulation_state(state);
}

bool SimulationEngine::validate_simulation_state(const state_vector::GeneralState* state) const {
    if (!state) {
        LOG_ERROR("Invalid state pointer", "SimulationEngine");
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
    }

    if (state->velocity()) {
        if (std::isnan(state->velocity()->x()) ||
            std::isnan(state->velocity()->y()) ||
            std::isnan(state->velocity()->z())) {
            LOG_ERROR("NaN detected in velocity", "SimulationEngine");
            return false;
        }

        double vel_x = state->velocity()->x();
        double vel_y = state->velocity()->y();
        double vel_z = state->velocity()->z();
        double speed = std::sqrt(vel_x*vel_x + vel_y*vel_y + vel_z*vel_z);

        const double ESCAPE_VELOCITY = 11200.0;
        if (speed > ESCAPE_VELOCITY * 2.0) {
            LOG_WARNING("Extreme velocity detected: " + std::to_string(speed) + " m/s (Mach " +
                       std::to_string(speed/343.0) + ")", "SimulationEngine");
            LOG_WARNING("This may indicate numerical instability", "SimulationEngine");
        }
    }

    return true;
}

void SimulationEngine::print_performance_metrics() const { // PERFORMANCE METRICS SHOULD NOT BE MANAGED IN THE SIMULATOR BUT IN VISUALIZATION ENGINE
    if (!plugin_manager_) {
        return;
    }

    LOG_INFO("=== SIMULATION PERFORMANCE METRICS ===", "SimulationEngine");
    LOG_INFO("Total iterations: " + std::to_string(iteration_count_.load()), "SimulationEngine");
    LOG_INFO("Simulation time: " + std::to_string(simulation_time_.load()) + "s", "SimulationEngine");
    LOG_INFO("Last compute tick duration: " + std::to_string(compute_tick_duration_ms_.load()) + "ms", "SimulationEngine");
    LOG_INFO("Last I/O tick duration: " + std::to_string(io_tick_duration_ms_.load()) + "ms", "SimulationEngine");
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

} // namespace MoLab
