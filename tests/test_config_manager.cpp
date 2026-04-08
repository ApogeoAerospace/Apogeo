#include <gtest/gtest.h>
#include "ConfigManager.h"
#include <fstream>
#include <nlohmann/json.hpp>

/**
 * @file test_config_manager.cpp
 * @brief Unit tests for `ConfigManager`.
 */

using json = nlohmann::json;
using namespace MoLab;

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
    // Create a temporary configuration file for tests
        test_config_file = "test_config.json";
        json test_config = {
            {"simulation", {
                {"duration", 100.0},
                {"max_iterations", 10000},
                {"enable_logging", true},
                {"log_file", "logs/test.log"},
                {"log_level", "INFO"}
            }},
            // Physics section removed per new config design
            {"initial_state_file", "data/initial_state.json"},
            {"output_directory", "output/"}
        };

        std::ofstream file(test_config_file);
        file << test_config.dump(4);
        file.close();
    }

    void TearDown() override {
    // Clean up temporary file
        std::remove(test_config_file.c_str());
    }

    std::string test_config_file;
};

TEST_F(ConfigManagerTest, LoadValidConfig) {
    auto& config_manager = ConfigManager::getInstance();
    EXPECT_TRUE(config_manager.loadConfig(test_config_file));
}

TEST_F(ConfigManagerTest, GetDuration) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    const auto& sim_config = config_manager.getSimulationConfig();
    EXPECT_DOUBLE_EQ(sim_config.simulation_duration, 100.0);
}

TEST_F(ConfigManagerTest, GetMaxIterations) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    const auto& sim_config = config_manager.getSimulationConfig();
    EXPECT_EQ(sim_config.max_iterations, 10000);
}

TEST_F(ConfigManagerTest, InvalidConfigFileUsesDefaults) {
    auto& config_manager = ConfigManager::getInstance();
    // loadConfig returns false if file does not exist, but applies defaults
    EXPECT_FALSE(config_manager.loadConfig("non_existent_file.json"));

    const auto& sim_config = config_manager.getSimulationConfig();
    EXPECT_EQ(sim_config.simulation_duration, 100.0);
}

TEST_F(ConfigManagerTest, ValidateConfigSuccess) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    std::string error_detail;
    EXPECT_TRUE(config_manager.validateConfig(error_detail));
}

TEST_F(ConfigManagerTest, GetInitialStateFile) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    EXPECT_EQ(config_manager.getInitialStateFile(), "data/initial_state.json");
}

TEST_F(ConfigManagerTest, GetOutputDirectory) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    EXPECT_EQ(config_manager.getOutputDirectory(), "output/");
}

TEST_F(ConfigManagerTest, LoggingEnabled) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    const auto& sim_config = config_manager.getSimulationConfig();
    EXPECT_TRUE(sim_config.enable_logging);
}

TEST_F(ConfigManagerTest, LogFileConfig) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    const auto& sim_config = config_manager.getSimulationConfig();
    EXPECT_EQ(sim_config.log_file, "logs/test.log");
}

TEST_F(ConfigManagerTest, LogLevelConfig) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    const auto& sim_config = config_manager.getSimulationConfig();
    EXPECT_EQ(sim_config.log_level, "INFO");
}

TEST_F(ConfigManagerTest, ConfigWithDifferentValues) {
    // Create a different config without physics section
    std::string alt_config = "alt_test_config.json";
    json alt_test_config = {
        {"simulation", {
            {"duration", 200.0},
            {"max_iterations", 20000},
            {"enable_logging", false},
            {"log_file", "logs/alt.log"},
            {"log_level", "DEBUG"}
        }},
        {"initial_state_file", "data/alt_state.json"},
        {"output_directory", "alt_output/"}
    };

    std::ofstream file(alt_config);
    file << alt_test_config.dump(4);
    file.close();

    auto& config_manager = ConfigManager::getInstance();
    EXPECT_TRUE(config_manager.loadConfig(alt_config));

    const auto& sim_config = config_manager.getSimulationConfig();
    EXPECT_DOUBLE_EQ(sim_config.simulation_duration, 200.0);
    EXPECT_EQ(sim_config.max_iterations, 20000);
    EXPECT_FALSE(sim_config.enable_logging);
    EXPECT_EQ(sim_config.log_file, "logs/alt.log");
    EXPECT_EQ(sim_config.log_level, "DEBUG");

    // Physics section removed: no physics_config usage here

    std::remove(alt_config.c_str());
}
