#include <gtest/gtest.h>
#include "../core/SimulationEngine.h"
#include "../src/core/ConfigManager.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>

using json = nlohmann::json;
namespace fs = std::filesystem;

class SimulationEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test directories
        fs::create_directories("test_data");
        fs::create_directories("test_output");
        fs::create_directories("test_logs");

        // Create test state file (new schema)
        test_state_file = "test_data/test_state.json";
        json test_state = {
            {"sim_time", 0.0},
            {"dt", 0.01},
            {"position", {{"x", 0.0}, {"y", 0.0}, {"z", 1000.0}}},
            {"velocity", {{"x", 10.0}, {"y", 0.0}, {"z", 0.0}}},
            {"orientation", {{"x", 0.0}, {"y", 0.0}, {"z", 0.0}, {"w", 1.0}}},
            {"angular_velocity", {{"x", 0.0}, {"y", 0.0}, {"z", 0.0}}},
            {"total_mass", 1000.0},
            {"cg_location", {{"x", 0.0}, {"y", 0.0}, {"z", 0.0}}},
            {"inertia_tensor", {{"ixx", 10.0}, {"iyy", 12.0}, {"izz", 8.0}, {"ixy", 0.0}, {"ixz", 0.0}, {"iyz", 0.0}}},
            {"propellant_masses", json::array({200.0, 150.0})},
            {"mach_number", 0.3},
            {"dynamic_pressure", 15000.0},
            {"angle_of_attack", 5.0},
            {"sideslip_angle", 0.0},
            {"atm_density", 1.225},
            {"atm_pressure", 101325.0},
            {"atm_temperature", 288.15},
            {"wind_velocity", {{"x", 0.0}, {"y", 0.0}, {"z", 0.0}}},
            {"gravity", {{"x", 0.0}, {"y", -9.81}, {"z", 0.0}}},
            {"engines", json::array({ {{"throttle", 0.8}, {"tvc_angles", {{"x", 0.0}, {"y", 0.0}, {"z", 0.0}}}} }) },
            {"surface_deflections", json::array({0.0, 0.0, 0.0})}
        };

        std::ofstream state_file(test_state_file);
        state_file << test_state.dump(4);
        state_file.close();

        // Create test config file
        test_config_file = "test_data/test_config.json";
        json test_config = {
            {"simulation", {
                {"duration", 1.0},
                {"max_iterations", 100},
                {"enable_logging", true},
                {"log_file", "test_logs/test.log"},
                {"log_level", "INFO"}
            }},
            {"plugins", json::array()}, // keep plugins empty for tests
            {"initial_state_file", test_state_file},
            {"output_directory", "test_output/"}
        };

        std::ofstream config_file(test_config_file);
        config_file << test_config.dump(4);
        config_file.close();
    }

    void TearDown() override {
        // Clean up test files
        if (fs::exists("test_data")) {
            fs::remove_all("test_data");
        }
        if (fs::exists("test_output")) {
            fs::remove_all("test_output");
        }
        if (fs::exists("test_logs")) {
            fs::remove_all("test_logs");
        }
        if (fs::exists("output")) {
            fs::remove_all("output");
        }
    }

    std::string test_state_file;
    std::string test_config_file;
};

TEST_F(SimulationEngineTest, CreateEngine) {
    SimulationEngine engine;
    EXPECT_EQ(engine.get_simulation_time(), 0.0);
    EXPECT_EQ(engine.get_iteration_count(), 0);
    EXPECT_FALSE(engine.is_running());
}

TEST_F(SimulationEngineTest, InitializeWithStateFile) {
    SimulationEngine engine;
    EXPECT_TRUE(engine.initialize(test_state_file));
    EXPECT_EQ(engine.get_simulation_time(), 0.0);
    EXPECT_EQ(engine.get_iteration_count(), 0);
}

TEST_F(SimulationEngineTest, InitializeWithNonExistentFile) {
    SimulationEngine engine;
    EXPECT_FALSE(engine.initialize("non_existent_state.json"));
}

TEST_F(SimulationEngineTest, InitializeWithConfig) {
    SimulationEngine engine;
    EXPECT_TRUE(engine.initialize_with_config(test_config_file));
}

TEST_F(SimulationEngineTest, GetSimulationTime) {
    SimulationEngine engine;
    engine.initialize(test_state_file);
    EXPECT_DOUBLE_EQ(engine.get_simulation_time(), 0.0);
}

TEST_F(SimulationEngineTest, GetIterationCount) {
    SimulationEngine engine;
    engine.initialize(test_state_file);
    EXPECT_EQ(engine.get_iteration_count(), 0);
}

TEST_F(SimulationEngineTest, ValidateInitialState) {
    SimulationEngine engine;
    engine.initialize(test_state_file);
    EXPECT_TRUE(engine.validate_simulation_state());
}

TEST_F(SimulationEngineTest, RunSingleTick) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    EXPECT_NO_THROW(engine.run_tick());
    EXPECT_TRUE(engine.is_running());
    EXPECT_EQ(engine.get_iteration_count(), 1);
    EXPECT_GT(engine.get_simulation_time(), 0.0);
}

TEST_F(SimulationEngineTest, RunMultipleTicks) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    for (int i = 0; i < 5; i++) {
        engine.run_tick();
    }

    EXPECT_TRUE(engine.is_running());
    EXPECT_EQ(engine.get_iteration_count(), 5);
}

TEST_F(SimulationEngineTest, ShutdownEngine) {
    SimulationEngine engine;
    engine.initialize(test_state_file);
    EXPECT_NO_THROW(engine.shutdown());
    EXPECT_FALSE(engine.is_running());
}

TEST_F(SimulationEngineTest, LoadPluginWithInvalidType) {
    SimulationEngine engine;
    engine.initialize(test_state_file);
    EXPECT_NO_THROW(engine.load_plugin("test_plugin", 999));
}

TEST_F(SimulationEngineTest, LoadPluginWithValidTypes) {
    SimulationEngine engine;
    engine.initialize(test_state_file);
    EXPECT_NO_THROW(engine.load_plugin("test_plugin", 0));
    EXPECT_NO_THROW(engine.load_plugin("test_plugin", 1));
}

TEST_F(SimulationEngineTest, GetLastTickDuration) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    engine.run_tick();
    EXPECT_GE(engine.get_last_tick_duration(), 0.0);
}

TEST_F(SimulationEngineTest, PrintPerformanceMetrics) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    engine.run_tick();
    EXPECT_NO_THROW(engine.print_performance_metrics());
}

TEST_F(SimulationEngineTest, RunFullSimulation) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    EXPECT_TRUE(engine.run_simulation());
    EXPECT_GT(engine.get_iteration_count(), 0);
}

TEST_F(SimulationEngineTest, ValidateStateAfterTick) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    engine.run_tick();
    EXPECT_TRUE(engine.validate_simulation_state());
}

// Ground Collision Se maneja en el módulo de ambiente, no en el núcleo de simulación

TEST_F(SimulationEngineTest, MultipleInitializations) {
    SimulationEngine engine;

    EXPECT_TRUE(engine.initialize(test_state_file));
    EXPECT_TRUE(engine.initialize(test_state_file));
    EXPECT_TRUE(engine.validate_simulation_state());
}

TEST_F(SimulationEngineTest, ShutdownWithoutInitialization) {
    SimulationEngine engine;
    EXPECT_NO_THROW(engine.shutdown());
}

TEST_F(SimulationEngineTest, TickWithoutInitialization) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);

    EXPECT_TRUE(engine.initialize(test_state_file));
    EXPECT_NO_THROW(engine.run_tick());
}

TEST_F(SimulationEngineTest, StateValidationWithEmptyBuffer) {
    SimulationEngine engine;
    EXPECT_FALSE(engine.validate_simulation_state());
}

TEST_F(SimulationEngineTest, SimulationTimeProgression) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    double initial_time = engine.get_simulation_time();
    engine.run_tick();
    double after_tick_time = engine.get_simulation_time();

    EXPECT_GT(after_tick_time, initial_time);
}

TEST_F(SimulationEngineTest, IterationCountIncrement) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    uint64_t initial_count = engine.get_iteration_count();
    engine.run_tick();
    engine.run_tick();
    uint64_t after_ticks_count = engine.get_iteration_count();

    EXPECT_EQ(after_ticks_count, initial_count + 2);
}

TEST_F(SimulationEngineTest, IsRunningAfterTick) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    EXPECT_FALSE(engine.is_running());
    engine.run_tick();
    EXPECT_TRUE(engine.is_running());
}

TEST_F(SimulationEngineTest, IsRunningAfterShutdown) {
    SimulationEngine engine;
    auto& config = MoLab::ConfigManager::getInstance();
    config.loadConfig(test_config_file);
    engine.initialize(test_state_file);

    engine.run_tick();
    EXPECT_TRUE(engine.is_running());
    engine.shutdown();
    EXPECT_FALSE(engine.is_running());
}
