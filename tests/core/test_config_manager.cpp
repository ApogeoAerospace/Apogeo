#include <gtest/gtest.h>
#include "ConfigManager.h"
#include <fstream>
#include <nlohmann/json.hpp>

/**
 * @file test_config_manager.cpp
 * @brief Unit tests for `ConfigManager`.
 */

using json = nlohmann::json;
using namespace Apogeo;

TEST(ConfigManagerRoundTrip, SavesPluginsLoggingAndStatePaths) {
    const std::string input = "config-roundtrip-input.json";
    const std::string saved = "config-roundtrip-saved.json";
    const json configuration = {
        {"simulation", {{"duration", 2}, {"max_iterations", 10}}},
        {"logging", {{"console_output", false}, {"file_output", false}}},
        {"vehicle_models", {{"flight_computer", {{"script_path", "flight.lua"}}}}},
        {"plugins", {{{"name", "test"}, {"library_path", "plugins/test"}, {"type", 1},
                      {"enabled", false}, {"parameters", {{"debug_output", true}}}}}},
        {"initial_state_file", "state.json"}, {"output_directory", "roundtrip-output"}
    };
    { std::ofstream file(input); file << configuration; }
    auto& manager = ConfigManager::getInstance();
    ASSERT_TRUE(manager.loadConfig(input));
    EXPECT_EQ(manager.flight_script_path, "flight.lua");
    ASSERT_TRUE(manager.saveConfig(saved));
    ASSERT_TRUE(manager.loadConfig(saved));
    EXPECT_EQ(manager.getInitialStateFile(), "state.json");
    EXPECT_EQ(manager.getOutputDirectory(), "roundtrip-output");
    ASSERT_EQ(manager.getPluginConfigs().size(), 1U);
    EXPECT_FALSE(manager.getPluginConfigs()[0].enabled);
    EXPECT_TRUE(manager.getPluginConfigs()[0].parameters["debug_output"].get<bool>());
    EXPECT_FALSE(manager.getSimulationConfig().file_output);
    EXPECT_FALSE(manager.saveConfig("missing-directory/config.json"));
    std::remove(input.c_str());
    std::remove(saved.c_str());
}

TEST(ConfigManagerValidation, RejectsMalformedSectionsAndInvalidPluginType) {
    auto& manager = ConfigManager::getInstance();
    const std::string input = "invalid-sections.json";
    for (const auto& configuration : std::vector<json>{
             {{"simulation", {{"duration", "bad"}}}},
             {{"plugins", {{{"name", "test"}, {"library_path", "test"}, {"type", "bad"}}}}},
             {{"simulation", {{"duration", 0}}}},
             {{"simulation", {{"max_iterations", 0}}}},
             {{"plugins", {{{"name", "test"}, {"library_path", "test"}, {"type", 9}}}}}}) {
        { std::ofstream file(input); file << configuration; }
        EXPECT_FALSE(manager.loadConfig(input)) << configuration.dump();
    }
    { std::ofstream file(input); file << "not json"; }
    EXPECT_FALSE(manager.loadConfig(input));
    std::remove(input.c_str());
}

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
            {"ipc", {
                {"tick_event_interval", 40},
                {"telemetry_interval_ticks", 8}
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
        {"ipc", {
            {"tick_event_interval", 25},
            {"telemetry_interval_ticks", 4}
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
    EXPECT_EQ(sim_config.ipc_tick_event_interval, 25);
    EXPECT_EQ(sim_config.ipc_telemetry_interval_ticks, 4);

    // Physics section removed: no physics_config usage here

    std::remove(alt_config.c_str());
}
