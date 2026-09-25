#include "ConfigManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include <fstream>
#include <iostream>

/**
 * @file ConfigManager.cpp
 * @brief Implementation of the MoLab configuration manager.
 */

namespace MoLab {

ConfigManager::ConfigManager() {
    setDefaults();
    config_loaded_ = false;
}

bool ConfigManager::loadConfig(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            // Use default values.
            setDefaults();
            config_loaded_ = false;
            logLoadFailure("Config file not found, using defaults: " + config_file);
            return false; // Defaults are applied, but no file was loaded
        }

        nlohmann::json config_json;
        file >> config_json;
        file.close();

        std::string error_detail;

        // Parse main sections
        if (!parseSimulationConfig(config_json, error_detail)) {
            setDefaults();
            config_loaded_ = false;
            logLoadFailure("Failed to parse simulation config: " + error_detail + ". Using defaults");
            return false;
        }
        if (!parsePluginConfigs(config_json, error_detail)) {
            setDefaults();
            config_loaded_ = false;
            logLoadFailure("Failed to parse plugin config: " + error_detail + ". Using defaults");
            return false;
        }

        // File paths
        if (config_json.contains("initial_state_file")) {
            initial_state_file_ = config_json["initial_state_file"];
        } else {
            initial_state_file_ = "data/default_state.json";
        }

        if (config_json.contains("output_directory")) {
            output_directory_ = config_json["output_directory"];
        } else {
            output_directory_ = "output/";
        }

        // Reset before parsing so a missing section in a new config clears a previous value.
        flight_script_path = "";
        if (config_json.contains("vehicle_models") && config_json["vehicle_models"].contains("flight_computer")) {
            flight_script_path = config_json["vehicle_models"]["flight_computer"]["script_path"];
        }

        // Validate overall consistency
        if (!validateConfig(error_detail)) {
            setDefaults();
            config_loaded_ = false;
            logLoadFailure("Configuration validation failed: " + error_detail + ". Using defaults");
            return false;
        }

        config_loaded_ = true;
        // Intentionally no runtime side effects here.
        // Logger bootstrap (console/file/level) is owned by main.cpp so
        // ConfigManager stays focused on load/parse/validate responsibilities.
        return true;

    } catch (const std::exception& e) {
        // Read/parse failure: use defaults as fallback
        setDefaults();
        config_loaded_ = false;
        logLoadFailure("Error loading config: " + std::string(e.what()));
        return false;
    }
}

bool ConfigManager::saveConfig(const std::string& config_file) const {
    try {
        nlohmann::json config_json;

        // Simulation configuration
        config_json["simulation"]["duration"] = simulation_config_.simulation_duration;
        config_json["simulation"]["max_iterations"] = simulation_config_.max_iterations;
        config_json["simulation"]["enable_logging"] = simulation_config_.enable_logging;
        config_json["simulation"]["log_file"] = simulation_config_.log_file;
        config_json["simulation"]["log_level"] = simulation_config_.log_level;
        config_json["ipc"]["tick_event_interval"] = simulation_config_.ipc_tick_event_interval;
        config_json["ipc"]["telemetry_interval_ticks"] = simulation_config_.ipc_telemetry_interval_ticks;
        config_json["logging"]["console_output"] = simulation_config_.console_output;
        config_json["logging"]["file_output"] = simulation_config_.file_output;

        // Plugin configuration
        for (const auto& plugin : plugin_configs_) {
            nlohmann::json plugin_json;
            plugin_json["name"] = plugin.name;
            plugin_json["type"] = plugin.type;
            plugin_json["library_path"] = plugin.library_path;
            plugin_json["enabled"] = plugin.enabled;
            plugin_json["parameters"] = plugin.parameters;
            config_json["plugins"].push_back(plugin_json);
        }

        // File paths
        config_json["initial_state_file"] = initial_state_file_;
        config_json["output_directory"] = output_directory_;

        std::ofstream file(config_file);
        if (!file.is_open()) {
            LOG_ERROR("Cannot open config file for writing: " + config_file, "ConfigManager");
            return false;
        }

        file << config_json.dump(4);
        file.close();

        LOG_INFO("Configuration saved to: " + config_file, "ConfigManager");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Error saving config: " + std::string(e.what()), "ConfigManager");
        return false;
    }
}

void ConfigManager::setDefaults() {
    // Default simulation values
    simulation_config_.simulation_duration = 100.0;
    simulation_config_.max_iterations = 10000;
    simulation_config_.enable_logging = true;
    simulation_config_.log_file = "logs/molab.log";
    simulation_config_.log_level = "INFO";
    simulation_config_.console_output = true;
    simulation_config_.file_output = true;
    simulation_config_.ipc_tick_event_interval = 50;
    simulation_config_.ipc_telemetry_interval_ticks = 5;

    // Clear plugin configuration
    plugin_configs_.clear();

    // Default paths
    initial_state_file_ = "data/default_state.json";
    output_directory_ = "output/";
    flight_script_path = "";

    // No ConfigManager logs before load.
}

bool ConfigManager::parseSimulationConfig(const nlohmann::json& json, std::string& error_detail) {
    try {
        if (json.contains("simulation")) {
            const auto& sim = json["simulation"];
            simulation_config_.simulation_duration = sim.value("duration", 100.0);
            simulation_config_.max_iterations = sim.value("max_iterations", 10000);
            simulation_config_.enable_logging = sim.value("enable_logging", true);
            simulation_config_.log_file = sim.value("log_file", "logs/molab.log");
            simulation_config_.log_level = sim.value("log_level", "INFO");
        } else {
            // If section is missing, use defaults
            simulation_config_.simulation_duration = 100.0;
            simulation_config_.max_iterations = 10000;
            simulation_config_.enable_logging = true;
            simulation_config_.log_file = "logs/molab.log";
            simulation_config_.log_level = "INFO";
        }

        if (json.contains("logging") && json["logging"].is_object()) {
            simulation_config_.console_output = json["logging"].value("console_output", true);
            simulation_config_.file_output = json["logging"].value("file_output", true);
        } else {
            simulation_config_.console_output = true;
            simulation_config_.file_output = true;
        }

        if (json.contains("ipc") && json["ipc"].is_object()) {
            simulation_config_.ipc_tick_event_interval = json["ipc"].value("tick_event_interval", 50);
            simulation_config_.ipc_telemetry_interval_ticks = json["ipc"].value("telemetry_interval_ticks", 5);
        } else {
            simulation_config_.ipc_tick_event_interval = 50;
            simulation_config_.ipc_telemetry_interval_ticks = 5;
        }

        return true;
    } catch (const std::exception& e) {
        error_detail = e.what();
        return false;
    }
}

bool ConfigManager::parsePluginConfigs(const nlohmann::json& json, std::string& error_detail) {
    try {
        plugin_configs_.clear();

        if (json.contains("plugins") && json["plugins"].is_array()) {
            for (const auto& plugin_json : json["plugins"]) {
                PluginConfig plugin;
                plugin.name = plugin_json.value("name", "");
                plugin.type = plugin_json.value("type", 0);
                plugin.library_path = plugin_json.value("library_path", "");
                plugin.enabled = plugin_json.value("enabled", true);

                if (plugin_json.contains("parameters")) {
                    plugin.parameters = plugin_json["parameters"];
                }

                // Add only if name and valid path are present
                if (!plugin.name.empty() && !plugin.library_path.empty()) {
                    plugin_configs_.push_back(plugin);
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        error_detail = e.what();
        return false;
    }
}

bool ConfigManager::validateConfig(std::string& error_detail) const {
    // Basic simulation validation
    if (simulation_config_.simulation_duration <= 0) {
        error_detail = "simulation.duration must be positive (got " + std::to_string(simulation_config_.simulation_duration) + ")";
        return false;
    }

    if (simulation_config_.max_iterations <= 0) {
        error_detail = "simulation.max_iterations must be positive (got " + std::to_string(simulation_config_.max_iterations) + ")";
        return false;
    }

    if (simulation_config_.ipc_tick_event_interval <= 0) {
        error_detail = "ipc.tick_event_interval must be positive (got " + std::to_string(simulation_config_.ipc_tick_event_interval) + ")";
        return false;
    }

    if (simulation_config_.ipc_telemetry_interval_ticks <= 0) {
        error_detail = "ipc.telemetry_interval_ticks must be positive (got " + std::to_string(simulation_config_.ipc_telemetry_interval_ticks) + ")";
        return false;
    }

    // Plugin validation
    for (const auto& plugin : plugin_configs_) {
        if (plugin.name.empty()) {
            error_detail = "plugin name cannot be empty";
            return false;
        }

        if (plugin.library_path.empty()) {
            error_detail = "plugin library_path cannot be empty";
            return false;
        }

        if (plugin.type < 0 || plugin.type > 1) {
            error_detail = "plugin type must be 0 or 1 (got " + std::to_string(plugin.type) + ")";
            return false;
        }
    }

    return true;
}

void ConfigManager::logLoadFailure(const std::string& message) {
    auto& logger = Logger::getInstance();

    // On configuration load failure, force full logger visibility so diagnostics
    // are not lost even if console output was disabled in previous runtime state.
    logger.setConsoleOutputEnabled(true);
    logger.setLogLevel(LogLevel::DEBUG);
    if (!simulation_config_.log_file.empty()) {
        logger.setLogFile(simulation_config_.log_file);
    }

    logger.error(message, "ConfigManager");
}

} // namespace MoLab
