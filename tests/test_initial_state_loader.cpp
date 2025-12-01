#include <gtest/gtest.h>
#include "../core/InitialStateLoader.h"
#include "state_vector_generated.h"
#include <flatbuffers/flatbuffers.h>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class InitialStateLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_state_file = "test_initial_state.json";

        // Create a valid test state file
        json test_state = {
            {"position", {{"x", 0.0}, {"y", 0.0}, {"z", 1000.0}}},
            {"velocity", {{"x", 10.0}, {"y", 0.0}, {"z", 0.0}}},
            {"orientation", {{"x", 0.0}, {"y", 0.0}, {"z", 0.0}, {"w", 1.0}}},
            {"atm_density", 1.225},
            {"atm_pressure", 101325.0},
            {"atm_temperature", 288.15},
            {"gravity", {{"x", 0.0}, {"y", -9.81}, {"z", 0.0}}},
            {"UTC", 1000000000},
            {"Time", 0.0},
            {"wind_speed", {{"x", 0.0}, {"y", 0.0}, {"z", 0.0}}}
        };

        std::ofstream file(test_state_file);
        file << test_state.dump(4);
        file.close();
    }

    void TearDown() override {
        std::remove(test_state_file.c_str());
    }

    std::string test_state_file;
};

TEST_F(InitialStateLoaderTest, LoadValidStateFile) {
    flatbuffers::FlatBufferBuilder builder;
    EXPECT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));
    EXPECT_GT(builder.GetSize(), 0u);
}

TEST_F(InitialStateLoaderTest, LoadNonExistentFile) {
    flatbuffers::FlatBufferBuilder builder;
    EXPECT_FALSE(InitialStateLoader::create_state_from_json(builder, "non_existent.json"));
}

TEST_F(InitialStateLoaderTest, LoadInvalidJSON) {
    std::string invalid_file = "invalid_state.json";
    std::ofstream file(invalid_file);
    file << "{ invalid json content }";
    file.close();

    flatbuffers::FlatBufferBuilder builder;
    EXPECT_FALSE(InitialStateLoader::create_state_from_json(builder, invalid_file));

    std::remove(invalid_file.c_str());
}

TEST_F(InitialStateLoaderTest, ValidateLoadedPosition) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->position(), nullptr);

    EXPECT_DOUBLE_EQ(state->position()->x(), 0.0);
    EXPECT_DOUBLE_EQ(state->position()->y(), 0.0);
    EXPECT_DOUBLE_EQ(state->position()->z(), 1000.0);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedVelocity) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->velocity(), nullptr);

    EXPECT_DOUBLE_EQ(state->velocity()->x(), 10.0);
    EXPECT_DOUBLE_EQ(state->velocity()->y(), 0.0);
    EXPECT_DOUBLE_EQ(state->velocity()->z(), 0.0);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedOrientation) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->orientation(), nullptr);

    EXPECT_DOUBLE_EQ(state->orientation()->x(), 0.0);
    EXPECT_DOUBLE_EQ(state->orientation()->y(), 0.0);
    EXPECT_DOUBLE_EQ(state->orientation()->z(), 0.0);
    EXPECT_DOUBLE_EQ(state->orientation()->w(), 1.0);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedAtmosphere) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_DOUBLE_EQ(state->atm_density(), 1.225);
    EXPECT_DOUBLE_EQ(state->atm_pressure(), 101325.0);
    EXPECT_DOUBLE_EQ(state->atm_temperature(), 288.15);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedGravity) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->gravity(), nullptr);

    EXPECT_DOUBLE_EQ(state->gravity()->x(), 0.0);
    EXPECT_DOUBLE_EQ(state->gravity()->y(), -9.81);
    EXPECT_DOUBLE_EQ(state->gravity()->z(), 0.0);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedWindSpeed) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->wind_speed(), nullptr);

    EXPECT_DOUBLE_EQ(state->wind_speed()->x(), 0.0);
    EXPECT_DOUBLE_EQ(state->wind_speed()->y(), 0.0);
    EXPECT_DOUBLE_EQ(state->wind_speed()->z(), 0.0);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedTimeData) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_DOUBLE_EQ(state->UTC(), 1000000000);
    EXPECT_DOUBLE_EQ(state->Time(), 0.0);
}

TEST_F(InitialStateLoaderTest, LoadWithDifferentValues) {
    std::string custom_file = "custom_state.json";
    json custom_state = {
        {"position", {{"x", 100.0}, {"y", 200.0}, {"z", 5000.0}}},
        {"velocity", {{"x", 50.0}, {"y", -10.0}, {"z", 20.0}}},
        {"orientation", {{"x", 0.707}, {"y", 0.0}, {"z", 0.0}, {"w", 0.707}}},
        {"atm_density", 0.5},
        {"atm_pressure", 50000.0},
        {"atm_temperature", 250.0},
        {"gravity", {{"x", 0.0}, {"y", -9.5}, {"z", 0.0}}},
        {"UTC", 2000000000},
        {"Time", 100.0},
        {"wind_speed", {{"x", 10.0}, {"y", 5.0}, {"z", 2.0}}}
    };

    std::ofstream file(custom_file);
    file << custom_state.dump(4);
    file.close();

    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, custom_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_DOUBLE_EQ(state->position()->x(), 100.0);
    EXPECT_DOUBLE_EQ(state->velocity()->y(), -10.0);
    EXPECT_DOUBLE_EQ(state->atm_density(), 0.5);
    EXPECT_DOUBLE_EQ(state->UTC(), 2000000000);

    std::remove(custom_file.c_str());
}
