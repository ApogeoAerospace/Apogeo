#include <gtest/gtest.h>
#include "SimulationEngine.h"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

/**
 * @file test_persistence.cpp
 * @brief End-to-end integration tests for SimulationEngine save and atomic load.
 */

namespace fs = std::filesystem;
using json = nlohmann::json;
using Apogeo::SimulationEngine;

class PersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        fs::create_directories("test_persistence_data");
        test_state_file = "test_persistence_data/state.json";
        test_save_file = "test_persistence_data/save.apogeo";

        json state_json = {
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
            {"gravity", {{"x", 0.0}, {"y", 0.0}, {"z", -9.81}}},
            {"engines", json::array({ {{"throttle", 0.5}, {"tvc_angles", {{"x", 0.0}, {"y", 0.0}, {"z", 0.0}}}} }) },
            {"surface_deflections", json::array({0.0, 0.0, 0.0})}
        };

        std::ofstream sf(test_state_file);
        sf << state_json.dump(4);
        sf.close();
    }

    void TearDown() override {
        if (fs::exists("test_persistence_data")) {
            fs::remove_all("test_persistence_data");
        }
    }

    std::string test_state_file;
    std::string test_save_file;
};

TEST_F(PersistenceTest, EngineSaveAndLoadRoundtrip) {
    SimulationEngine engine;
    ASSERT_TRUE(engine.initialize(test_state_file));

    for (int i = 0; i < 5; ++i) {
        engine.run_tick();
    }
    double sim_time_before = engine.get_simulation_time();
    uint64_t iter_before = engine.get_iteration_count();

    EXPECT_TRUE(engine.save_state(test_save_file));
    EXPECT_TRUE(fs::exists(test_save_file));

    SimulationEngine restored_engine;
    ASSERT_TRUE(restored_engine.initialize(test_state_file));
    EXPECT_TRUE(restored_engine.load_state(test_save_file));

    EXPECT_DOUBLE_EQ(restored_engine.get_simulation_time(), sim_time_before);
    EXPECT_EQ(restored_engine.get_iteration_count(), iter_before);
    EXPECT_TRUE(restored_engine.validate_simulation_state());
}

TEST_F(PersistenceTest, ContinuousVsDividedExecutionEquivalence) {
    // 10 ticks continuously
    SimulationEngine cont_engine;
    ASSERT_TRUE(cont_engine.initialize(test_state_file));
    for (int i = 0; i < 10; ++i) {
        cont_engine.run_tick();
    }
    double cont_final_time = cont_engine.get_simulation_time();
    uint64_t cont_final_iter = cont_engine.get_iteration_count();

    // 5 ticks -> save -> load -> 5 more ticks
    SimulationEngine div_engine;
    ASSERT_TRUE(div_engine.initialize(test_state_file));
    for (int i = 0; i < 5; ++i) {
        div_engine.run_tick();
    }
    ASSERT_TRUE(div_engine.save_state(test_save_file));

    SimulationEngine resumed_engine;
    ASSERT_TRUE(resumed_engine.initialize(test_state_file));
    ASSERT_TRUE(resumed_engine.load_state(test_save_file));

    for (int i = 0; i < 5; ++i) {
        resumed_engine.run_tick();
    }

    EXPECT_DOUBLE_EQ(resumed_engine.get_simulation_time(), cont_final_time);
    EXPECT_EQ(resumed_engine.get_iteration_count(), cont_final_iter);
}

TEST_F(PersistenceTest, AtomicLoadFailureLeavesEngineIntact) {
    SimulationEngine engine;
    ASSERT_TRUE(engine.initialize(test_state_file));
    engine.run_tick();
    double time_before = engine.get_simulation_time();

    EXPECT_FALSE(engine.load_state("non_existent_file.apogeo"));
    EXPECT_DOUBLE_EQ(engine.get_simulation_time(), time_before);

    std::string corrupt_file = "test_persistence_data/corrupt.apogeo";
    std::ofstream cf(corrupt_file);
    cf << "{\"invalid\": true}";
    cf.close();

    EXPECT_FALSE(engine.load_state(corrupt_file));
    EXPECT_DOUBLE_EQ(engine.get_simulation_time(), time_before);
}
