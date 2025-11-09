#include <gtest/gtest.h>
#include "ConfigManager.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace MoLab;

class ConfigManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Crear un archivo de configuración temporal para tests
        test_config_file = "test_config.json";
        json test_config = {
            {"simulation", {
                {"time_step", 0.01},
                {"duration", 100.0},
                {"max_iterations", 10000},
                {"enable_logging", true},
                {"log_file", "logs/test.log"},
                {"log_level", "INFO"}
            }},
            {"physics", {
                {"enable_gravity", true},
                {"enable_atmospheric_drag", false},
                {"enable_wind_effects", false},
                {"integration_tolerance", 1e-6},
                {"integrator_type", "runge_kutta_4"}
            }},
            {"initial_state_file", "data/initial_state.json"},
            {"output_directory", "output/"}
        };

        std::ofstream file(test_config_file);
        file << test_config.dump(4);
        file.close();
    }

    void TearDown() override {
        // Limpiar archivo temporal
        std::remove(test_config_file.c_str());
    }

    std::string test_config_file;
};

TEST_F(ConfigManagerTest, LoadValidConfig) {
    auto& config_manager = ConfigManager::getInstance();
    EXPECT_TRUE(config_manager.loadConfig(test_config_file));
}

TEST_F(ConfigManagerTest, GetTimeStep) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    const auto& sim_config = config_manager.getSimulationConfig();
    EXPECT_DOUBLE_EQ(sim_config.time_step, 0.01);
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
    // loadConfig retorna false pero usa defaults, no lanza excepción
    EXPECT_FALSE(config_manager.loadConfig("non_existent_file.json"));

    // Debe tener valores por defecto
    const auto& sim_config = config_manager.getSimulationConfig();
    EXPECT_GT(sim_config.time_step, 0.0);
}

TEST_F(ConfigManagerTest, GravityEnabled) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    const auto& physics_config = config_manager.getPhysicsConfig();
    EXPECT_TRUE(physics_config.enable_gravity);
}

TEST_F(ConfigManagerTest, IntegratorType) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    const auto& physics_config = config_manager.getPhysicsConfig();
    EXPECT_EQ(physics_config.integrator_type, "runge_kutta_4");
}

TEST_F(ConfigManagerTest, ValidateConfigSuccess) {
    auto& config_manager = ConfigManager::getInstance();
    config_manager.loadConfig(test_config_file);

    EXPECT_TRUE(config_manager.validateConfig());
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
