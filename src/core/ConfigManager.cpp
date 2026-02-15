#include "ConfigManager.h"
#include "Logger.h"
#include <fstream>
#include <iostream>

namespace MoLab {

bool ConfigManager::loadConfig(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            LOG_WARNING("Config file not found, using defaults: " + config_file, "ConfigManager");
            setDefaults();
            return true; // Not an error, just use defaults
        }

        nlohmann::json config_json;
        file >> config_json;
        file.close();

        LOG_INFO("Loading configuration from: " + config_file, "ConfigManager");

        // Parse different sections
        if (!parseSimulationConfig(config_json)) {
            return false;
        }
        if (!parsePhysicsConfig(config_json)) {
            return false;
        }
        if (!parsePluginConfigs(config_json)) {
            return false;
        }

        // Parse output config
        if (!parseOutputConfig(config_json)) {
            return false;
        }

        // Parse file paths
        if (config_json.contains("initial_state_file")) {
            initial_state_file_ = config_json["initial_state_file"];
        } else {
            initial_state_file_ = "data/initial_state.json";
        }

        if (!validateConfig()) {
            LOG_ERROR("Configuration validation failed", "ConfigManager");
            return false;
        }

        LOG_INFO("Configuration loaded successfully", "ConfigManager");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Error loading config: " + std::string(e.what()), "ConfigManager");
        setDefaults();
        return false;
    }
}

bool ConfigManager::saveConfig(const std::string& config_file) const {
    try {
        nlohmann::json config_json;

        // Simulation config
        config_json["simulation"]["time_step"] = simulation_config_.time_step;
        config_json["simulation"]["duration"] = simulation_config_.simulation_duration;
        config_json["simulation"]["max_iterations"] = simulation_config_.max_iterations;
        config_json["simulation"]["enable_logging"] = simulation_config_.enable_logging;
        config_json["simulation"]["log_file"] = simulation_config_.log_file;
        config_json["simulation"]["log_level"] = simulation_config_.log_level;

        // Physics config
        config_json["physics"]["enable_gravity"] = physics_config_.enable_gravity;
        config_json["physics"]["gravity_magnitude"] = physics_config_.gravity_magnitude;
        config_json["physics"]["enable_atmospheric_drag"] = physics_config_.enable_atmospheric_drag;
        config_json["physics"]["enable_wind_effects"] = physics_config_.enable_wind_effects;
        config_json["physics"]["integration_tolerance"] = physics_config_.integration_tolerance;
        config_json["physics"]["integrator_type"] = physics_config_.integrator_type;
        config_json["physics"]["vehicle_mass"] = physics_config_.vehicle_mass;
        config_json["physics"]["drag_coefficient"] = physics_config_.drag_coefficient;
        config_json["physics"]["reference_area"] = physics_config_.reference_area;

        // Plugin configs
        for (const auto& plugin : plugin_configs_) {
            nlohmann::json plugin_json;
            plugin_json["name"] = plugin.name;
            plugin_json["type"] = plugin.type;
            plugin_json["library_path"] = plugin.library_path;
            plugin_json["enabled"] = plugin.enabled;
            plugin_json["parameters"] = plugin.parameters;
            config_json["plugins"].push_back(plugin_json);
        }

        // Output config
        config_json["output"]["output_directory"] = output_config_.output_directory;
        config_json["output"]["output_interval"] = output_config_.output_interval;
        config_json["output"]["enable_csv"] = output_config_.enable_csv;
        config_json["output"]["enable_json"] = output_config_.enable_json;
        config_json["output"]["enable_binary"] = output_config_.enable_binary;

        // File paths
        config_json["initial_state_file"] = initial_state_file_;

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
    // Simulation defaults
    simulation_config_.time_step = 0.01; // 10ms
    simulation_config_.simulation_duration = 100.0; // 100 seconds
    simulation_config_.max_iterations = 10000;
    simulation_config_.enable_logging = true;
    simulation_config_.log_file = "logs/molab.log";
    simulation_config_.log_level = "INFO";

    // Physics defaults
    physics_config_.enable_gravity = true;
    physics_config_.gravity_magnitude = 9.81;
    physics_config_.enable_atmospheric_drag = true;
    physics_config_.enable_wind_effects = false;
    physics_config_.integration_tolerance = 1e-6;
    physics_config_.integrator_type = "runge_kutta_4";
    physics_config_.vehicle_mass = 1000.0;
    physics_config_.drag_coefficient = 0.3;
    physics_config_.reference_area = 1.0;

    // Output defaults
    output_config_.output_directory = "output/";
    output_config_.output_interval = 5;
    output_config_.enable_csv = true;
    output_config_.enable_json = true;
    output_config_.enable_binary = false;

    plugin_configs_.clear();

    initial_state_file_ = "data/initial_state.json";

    LOG_INFO("Using default configuration", "ConfigManager");
}

bool ConfigManager::parseSimulationConfig(const nlohmann::json& json) {
    try {
        if (json.contains("simulation")) {
            const auto& sim = json["simulation"];

            simulation_config_.time_step = sim.value("time_step", 0.01);
            simulation_config_.simulation_duration = sim.value("duration", 100.0);
            simulation_config_.max_iterations = sim.value("max_iterations", 10000);
            simulation_config_.enable_logging = sim.value("enable_logging", true);
            simulation_config_.log_file = sim.value("log_file", "logs/molab.log");
            simulation_config_.log_level = sim.value("log_level", "INFO");
        } else {
            // Use defaults if section doesn't exist
            simulation_config_.time_step = 0.01;
            simulation_config_.simulation_duration = 100.0;
            simulation_config_.max_iterations = 10000;
            simulation_config_.enable_logging = true;
            simulation_config_.log_file = "logs/molab.log";
            simulation_config_.log_level = "INFO";
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing simulation config: " + std::string(e.what()), "ConfigManager");
        return false;
    }
}

bool ConfigManager::parsePhysicsConfig(const nlohmann::json& json) {
    try {
        if (json.contains("physics")) {
            const auto& physics = json["physics"];

            physics_config_.enable_gravity = physics.value("enable_gravity", true);
            physics_config_.gravity_magnitude = physics.value("gravity_magnitude", 9.81);
            physics_config_.enable_atmospheric_drag = physics.value("enable_atmospheric_drag", true);
            physics_config_.enable_wind_effects = physics.value("enable_wind_effects", false);
            physics_config_.integration_tolerance = physics.value("integration_tolerance", 1e-6);
            physics_config_.integrator_type = physics.value("integrator_type", "runge_kutta_4");
            physics_config_.vehicle_mass = physics.value("vehicle_mass", 1000.0);
            physics_config_.drag_coefficient = physics.value("drag_coefficient", 0.3);
            physics_config_.reference_area = physics.value("reference_area", 1.0);
        } else {
            setDefaults(); // Use full defaults
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing physics config: " + std::string(e.what()), "ConfigManager");
        return false;
    }
}

bool ConfigManager::parsePluginConfigs(const nlohmann::json& json) {
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

                if (!plugin.name.empty() && !plugin.library_path.empty()) {
                    plugin_configs_.push_back(plugin);
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing plugin configs: " + std::string(e.what()), "ConfigManager");
        return false;
    }
}

bool ConfigManager::parseOutputConfig(const nlohmann::json& json) {
    try {
        if (json.contains("output")) {
            const auto& output = json["output"];

            output_config_.output_directory = output.value("output_directory", "output/");
            output_config_.output_interval = output.value("output_interval", 5);
            output_config_.enable_csv = output.value("enable_csv", true);
            output_config_.enable_json = output.value("enable_json", true);
            output_config_.enable_binary = output.value("enable_binary", false);
        } else {
            // Use defaults
            output_config_.output_directory = "output/";
            output_config_.output_interval = 5;
            output_config_.enable_csv = true;
            output_config_.enable_json = true;
            output_config_.enable_binary = false;
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error parsing output config: " + std::string(e.what()), "ConfigManager");
        return false;
    }
}

bool ConfigManager::validateConfig() const {
    // Validate simulation config
    if (simulation_config_.time_step <= 0) {
        LOG_ERROR("Invalid time step: must be positive", "ConfigManager");
        return false;
    }

    if (simulation_config_.simulation_duration <= 0) {
        LOG_ERROR("Invalid simulation duration: must be positive", "ConfigManager");
        return false;
    }

    if (simulation_config_.max_iterations <= 0) {
        LOG_ERROR("Invalid max iterations: must be positive", "ConfigManager");
        return false;
    }

    // Validate physics config
    if (physics_config_.integration_tolerance <= 0) {
        LOG_ERROR("Invalid integration tolerance: must be positive", "ConfigManager");
        return false;
    }

    // Validate integrator type
    const std::vector<std::string> valid_integrators = {
        "euler", "runge_kutta_4", "verlet"
    };

    bool valid_integrator = false;
    for (const auto& integrator : valid_integrators) {
        if (physics_config_.integrator_type == integrator) {
            valid_integrator = true;
            break;
        }
    }

    if (!valid_integrator) {
        LOG_ERROR("Invalid integrator type: " + physics_config_.integrator_type, "ConfigManager");
        return false;
    }

    // Validate plugins
    for (const auto& plugin : plugin_configs_) {
        if (plugin.name.empty()) {
            LOG_ERROR("Plugin name cannot be empty", "ConfigManager");
            return false;
        }

        if (plugin.library_path.empty()) {
            LOG_ERROR("Plugin library path cannot be empty for: " + plugin.name, "ConfigManager");
            return false;
        }

        if (plugin.type < 0 || plugin.type > 1) {
            LOG_ERROR("Invalid plugin type for " + plugin.name + ": must be 0 or 1", "ConfigManager");
            return false;
        }
    }

    return true;
}

} // namespace MoLab
