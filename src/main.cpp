#include <iostream>
#include <string>
#include <filesystem>
#include "../core/SimulationEngine.h"
#include "../core/Logger.h"
#include "../core/ConfigManager.h"

/**
 * @file main.cpp
 * @brief Entry point for the MoLab application.
 */

/**
 * @brief Prints usage help to console.
 *
 * @param program_name Invoked executable name.
 */
void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  -c, --config <file>     Use specified configuration file\n";
    std::cout << "  -s, --state <file>      Use specified initial state file\n";
    std::cout << "  -t, --ticks <number>    Run specified number of ticks (default: full simulation)\n";
    std::cout << "  -l, --log-level <level> Set log level (DEBUG, INFO, WARNING, ERROR, CRITICAL) (default: INFO)\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  --version               Show version information\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " --config data/default_config.json\n";
    std::cout << "  " << program_name << " --state data/default_state.json --ticks 100\n";
}

/**
 * @brief Prints current simulator version.
 */
void print_version() {
    std::cout << "MoLab Aerospace Simulator v1.0.0\n";
    std::cout << "Built with C++17, FlatBuffers, and nlohmann_json\n";
    std::cout << "Copyright (c) 2026 MoLab Team\n";
}

/**
 * @brief Runs simulator initialization and main loop.
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument values.
 * @return `0` on successful execution, `1` on error.
 */
int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string config_file = "data/default_config.json";
    int tick_count = -1; // -1 means run full simulation
    std::string log_level = "INFO";

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--version") {
            print_version();
            return 0;
        } else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                config_file = argv[++i];
            } else {
                std::cerr << "Error: --config requires a file path\n";
                return 1;
            }
        } else if (arg == "-t" || arg == "--ticks") {
            if (i + 1 < argc) {
                try {
                    tick_count = std::stoi(argv[++i]);
                    if (tick_count <= 0) {
                        std::cerr << "Error: tick count must be positive\n";
                        return 1;
                    }
                } catch (const std::exception&) {
                    std::cerr << "Error: invalid tick count\n";
                    return 1;
                }
            } else {
                std::cerr << "Error: --ticks requires a number\n";
                return 1;
            }
        } else if (arg == "-l" || arg == "--log-level") {
            if (i + 1 < argc) {
                log_level = argv[++i];
            } else {
                std::cerr << "Error: --log-level requires a level\n";
                return 1;
            }
        } else {
            std::cerr << "Error: unknown option " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // Initialize logging
    auto& logger = MoLab::Logger::getInstance();
    if (log_level == "DEBUG") {
        logger.setLogLevel(MoLab::LogLevel::DEBUG);
    } else if (log_level == "INFO") {
        logger.setLogLevel(MoLab::LogLevel::INFO);
    } else if (log_level == "WARNING") {
        logger.setLogLevel(MoLab::LogLevel::WARNING);
    } else if (log_level == "ERROR") {
        logger.setLogLevel(MoLab::LogLevel::ERR);
    } else if (log_level == "CRITICAL") {
        logger.setLogLevel(MoLab::LogLevel::CRITICAL);
    }

    LOG_INFO("Starting MoLab Aerospace Simulator", "Main");
    LOG_INFO("Configuration file: " + config_file, "Main");

    // Check if config file exists
    if (!std::filesystem::exists(config_file)) {
        LOG_WARNING("Configuration file not found: " + config_file, "Main");
        LOG_INFO("Using default configuration", "Main");
    }

    try {
        // Initialize simulation engine
        MoLab::SimulationEngine engine;

        // Initialize with configuration
        bool init_success = engine.initialize_with_config(config_file);

        if (!init_success) {
            LOG_CRITICAL("Failed to initialize simulation engine", "Main");
            return 1;
        }

        LOG_INFO("Simulation engine initialized successfully", "Main");

        // Run simulation
        if (tick_count > 0) {
            LOG_INFO("Running " + std::to_string(tick_count) + " simulation ticks", "Main");

            // Determine progress frequency based on total ticks
            // More frequent for long runs, less frequent for short ones
            int progress_interval;
            if (tick_count <= 100) {
                progress_interval = 10;      // Every 10 ticks for short runs
            } else if (tick_count <= 1000) {
                progress_interval = 50;      // Every 50 ticks for medium runs
            } else if (tick_count <= 5000) {
                progress_interval = 100;     // Every 100 ticks for long runs
            } else {
                progress_interval = 250;     // Every 250 ticks for very long runs
            }

            for (int i = 0; i < tick_count; ++i) {
                // PROGRESS LOGGING: frequent and simple for external interface
                if (i % progress_interval == 0 || i == tick_count - 1) {
                    LOG_INFO("Executing tick " + std::to_string(i + 1) + " of " + std::to_string(tick_count), "Main");
                }
                engine.run_tick();
            }

            LOG_INFO("Completed " + std::to_string(tick_count) + " simulation ticks", "Main");
        } else {
            LOG_INFO("Running full simulation", "Main");
            if (!engine.run_simulation()) {
                LOG_ERROR("Simulation failed", "Main");
                return 1;
            }
        }

        // Shutdown
        engine.shutdown();
        LOG_INFO("MoLab Aerospace Simulator finished successfully", "Main");

        return 0;

    } catch (const std::exception& e) {
        LOG_CRITICAL("Unhandled exception: " + std::string(e.what()), "Main");
        return 1;
    } catch (...) {
        LOG_CRITICAL("Unknown exception occurred", "Main");
        return 1;
    }
}
