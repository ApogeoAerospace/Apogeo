#include <gtest/gtest.h>
#include "../src/FlightComputerContext.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

/**
 * @file test_flight_computer.cpp
 * @brief Unit tests for FlightComputerContext.
 */

using json = nlohmann::json;
namespace fs = std::filesystem;

TEST(FlightComputerTest, LoadScriptSuccessfully) {
    Apogeo::FlightComputerContext context("scripts/vuelo_test.lua");
    EXPECT_TRUE(context.is_valid());
}

TEST(FlightComputerTest, SaveAndRestoreMutableGlobals) {
    fs::create_directories("test_fc_data");
    const std::string script_path = "test_fc_data/stateful.lua";
    std::ofstream lf(script_path);
    lf << "burn_counter = 0\n"
       << "target_reached = false\n"
       << "flight_phase = \"ASCENT\"\n"
       << "function on_tick(dt)\n"
       << "    burn_counter = burn_counter + 1\n"
       << "    if burn_counter >= 5 then\n"
       << "        target_reached = true\n"
       << "        flight_phase = \"CRUISE\"\n"
       << "    end\n"
       << "end\n";
    lf.close();

    Apogeo::FlightComputerContext fc(script_path);
    ASSERT_TRUE(fc.is_valid());

    flatbuffers::FlatBufferBuilder init_builder;
    state_vector::GeneralStateBuilder gsb(init_builder);
    auto offset = gsb.Finish();
    init_builder.Finish(offset);
    const auto* valid_state = state_vector::GetGeneralState(init_builder.GetBufferPointer());

    flatbuffers::FlatBufferBuilder fbb1, fbb2;
    fc.update(valid_state, fbb1, 0.01f);
    fc.update(valid_state, fbb2, 0.01f);

    std::string saved_lua = fc.save_lua_globals();
    auto j = json::parse(saved_lua);

    EXPECT_EQ(j["burn_counter"].get<double>(), 2.0);
    EXPECT_EQ(j["target_reached"].get<bool>(), false);
    EXPECT_EQ(j["flight_phase"].get<std::string>(), "ASCENT");

    Apogeo::FlightComputerContext fresh_fc(script_path);
    ASSERT_TRUE(fresh_fc.is_valid());
    EXPECT_TRUE(fresh_fc.restore_lua_globals(saved_lua));

    for (int i = 0; i < 3; ++i) {
        flatbuffers::FlatBufferBuilder fbb;
        fresh_fc.update(valid_state, fbb, 0.01f);
    }

    std::string advanced_globals = fresh_fc.save_lua_globals();
    auto j3 = json::parse(advanced_globals);
    EXPECT_EQ(j3["burn_counter"].get<double>(), 5.0);
    EXPECT_EQ(j3["target_reached"].get<bool>(), true);
    EXPECT_EQ(j3["flight_phase"].get<std::string>(), "CRUISE");

    fs::remove_all("test_fc_data");
}

TEST(FlightComputerTest, SaveAndRestoreTableGlobals) {
    fs::create_directories("test_fc_data");
    const std::string script_path = "test_fc_data/table_stateful.lua";
    std::ofstream lf(script_path);
    lf << "pid = { kp = 1.2, ki = 0.05, kd = 0.1, integral = 0.0 }\n"
       << "waypoints = { {x = 100.0, y = 200.0}, {x = 300.0, y = 400.0} }\n"
       << "flight_modes = { \"STANDBY\", \"ASCENT\", \"CRUISE\" }\n"
       << "step = 0\n"
       << "function on_tick(dt)\n"
       << "    pid.integral = pid.integral + 0.5\n"
       << "    waypoints[1].x = waypoints[1].x + 10.0\n"
       << "    step = step + 1\n"
       << "end\n";
    lf.close();

    Apogeo::FlightComputerContext fc(script_path);
    ASSERT_TRUE(fc.is_valid());

    flatbuffers::FlatBufferBuilder init_builder;
    state_vector::GeneralStateBuilder gsb(init_builder);
    auto offset = gsb.Finish();
    init_builder.Finish(offset);
    const auto* valid_state = state_vector::GetGeneralState(init_builder.GetBufferPointer());

    flatbuffers::FlatBufferBuilder fbb1, fbb2;
    fc.update(valid_state, fbb1, 0.01f);
    fc.update(valid_state, fbb2, 0.01f);

    std::string saved_lua = fc.save_lua_globals();
    auto j = json::parse(saved_lua);

    EXPECT_DOUBLE_EQ(j["pid"]["kp"].get<double>(), 1.2);
    EXPECT_DOUBLE_EQ(j["pid"]["integral"].get<double>(), 1.0);
    EXPECT_DOUBLE_EQ(j["waypoints"][0]["x"].get<double>(), 120.0);
    EXPECT_DOUBLE_EQ(j["waypoints"][0]["y"].get<double>(), 200.0);
    EXPECT_EQ(j["flight_modes"][1].get<std::string>(), "ASCENT");
    EXPECT_EQ(j["step"].get<double>(), 2.0);

    // Restore into a fresh context
    Apogeo::FlightComputerContext fresh_fc(script_path);
    ASSERT_TRUE(fresh_fc.is_valid());
    EXPECT_TRUE(fresh_fc.restore_lua_globals(saved_lua));

    // Advance 1 more tick
    flatbuffers::FlatBufferBuilder fbb3;
    fresh_fc.update(valid_state, fbb3, 0.01f);

    std::string updated_saved = fresh_fc.save_lua_globals();
    auto j_fresh = json::parse(updated_saved);

    EXPECT_DOUBLE_EQ(j_fresh["pid"]["integral"].get<double>(), 1.5);
    EXPECT_DOUBLE_EQ(j_fresh["waypoints"][0]["x"].get<double>(), 130.0);
    EXPECT_EQ(j_fresh["step"].get<double>(), 3.0);

    fs::remove_all("test_fc_data");
}

TEST(FlightComputerTest, CircularReferenceTableHandledGracefully) {
    fs::create_directories("test_fc_data");
    const std::string script_path = "test_fc_data/circular.lua";
    std::ofstream lf(script_path);
    lf << "cycle = { name = \"loop\", val = 42 }\n"
       << "cycle.self = cycle\n"
       << "function on_tick(dt) end\n";
    lf.close();

    Apogeo::FlightComputerContext fc(script_path);
    ASSERT_TRUE(fc.is_valid());

    std::string saved_lua = fc.save_lua_globals();
    auto j = json::parse(saved_lua);

    EXPECT_EQ(j["cycle"]["val"].get<double>(), 42.0);
    EXPECT_EQ(j["cycle"]["name"].get<std::string>(), "loop");

    fs::remove_all("test_fc_data");
}

TEST(FlightComputerTest, RestoreErrorOnInvalidFormat) {
    Apogeo::FlightComputerContext fc("scripts/vuelo_test.lua");
    ASSERT_TRUE(fc.is_valid());

    // Root must be a JSON object, not an array
    EXPECT_FALSE(fc.restore_lua_globals("[1, 2, 3]"));

    // Malformed JSON syntax
    EXPECT_FALSE(fc.restore_lua_globals("{ invalid json syntax }"));

    // Unsupported value type (null)
    EXPECT_FALSE(fc.restore_lua_globals("{\"invalid_null\": null}"));
}
