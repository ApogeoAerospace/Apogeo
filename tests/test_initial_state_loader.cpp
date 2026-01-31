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

        // Create a valid test state file (new schema)
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
            {"wind_velocity", {{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}}},
            {"gravity", {{"x", 0.0f}, {"y", -9.81f}, {"z", 0.0f}}},
            {"engines", json::array({ {{"throttle", 0.8f}, {"tvc_angles", {{"x", 0.0f}, {"y", 0.0f}, {"z", 0.0f}}}} }) },
            {"surface_deflections", json::array({0.0f, 0.0f, 0.0f})}
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

    EXPECT_FLOAT_EQ(state->position()->x(), 0.0f);
    EXPECT_FLOAT_EQ(state->position()->y(), 0.0f);
    EXPECT_FLOAT_EQ(state->position()->z(), 1000.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedVelocity) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->velocity(), nullptr);

    EXPECT_FLOAT_EQ(state->velocity()->x(), 10.0f);
    EXPECT_FLOAT_EQ(state->velocity()->y(), 0.0f);
    EXPECT_FLOAT_EQ(state->velocity()->z(), 0.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedOrientation) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->orientation(), nullptr);

    EXPECT_FLOAT_EQ(state->orientation()->x(), 0.0f);
    EXPECT_FLOAT_EQ(state->orientation()->y(), 0.0f);
    EXPECT_FLOAT_EQ(state->orientation()->z(), 0.0f);
    EXPECT_FLOAT_EQ(state->orientation()->w(), 1.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedAngularVelocity) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->angular_velocity(), nullptr);

    EXPECT_FLOAT_EQ(state->angular_velocity()->x(), 0.0f);
    EXPECT_FLOAT_EQ(state->angular_velocity()->y(), 0.0f);
    EXPECT_FLOAT_EQ(state->angular_velocity()->z(), 0.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedTotalMass) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_FLOAT_EQ(state->total_mass(), 1000.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedCGLocation) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->cg_location(), nullptr);

    EXPECT_FLOAT_EQ(state->cg_location()->x(), 0.0f);
    EXPECT_FLOAT_EQ(state->cg_location()->y(), 0.0f);
    EXPECT_FLOAT_EQ(state->cg_location()->z(), 0.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedInertiaTensor) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->inertia_tensor(), nullptr);

    EXPECT_FLOAT_EQ(state->inertia_tensor()->ixx(), 10.0f);
    EXPECT_FLOAT_EQ(state->inertia_tensor()->iyy(), 12.0f);
    EXPECT_FLOAT_EQ(state->inertia_tensor()->izz(), 8.0f);
    EXPECT_FLOAT_EQ(state->inertia_tensor()->ixy(), 0.0f);
    EXPECT_FLOAT_EQ(state->inertia_tensor()->ixz(), 0.0f);
    EXPECT_FLOAT_EQ(state->inertia_tensor()->iyz(), 0.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedPropellantMasses) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->propellant_masses(), nullptr);

    ASSERT_EQ(state->propellant_masses()->size(), 2);
    EXPECT_FLOAT_EQ(state->propellant_masses()->Get(0), 200.0f);
    EXPECT_FLOAT_EQ(state->propellant_masses()->Get(1), 150.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedMachNumber) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_FLOAT_EQ(state->mach_number(), 0.3f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedDynamicPressure) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_FLOAT_EQ(state->dynamic_pressure(), 15000.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedAngleOfAttack) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_FLOAT_EQ(state->angle_of_attack(), 5.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedSideslipAngle) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_FLOAT_EQ(state->sideslip_angle(), 0.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedAtmosphere) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_FLOAT_EQ(state->atm_density(), 1.225f);
    EXPECT_FLOAT_EQ(state->atm_pressure(), 101325.0f);
    EXPECT_FLOAT_EQ(state->atm_temperature(), 288.15f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedGravity) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->gravity(), nullptr);

    EXPECT_FLOAT_EQ(state->gravity()->x(), 0.0f);
    EXPECT_FLOAT_EQ(state->gravity()->y(), -9.81f);
    EXPECT_FLOAT_EQ(state->gravity()->z(), 0.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedWindVelocity) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);
    ASSERT_NE(state->wind_velocity(), nullptr);

    EXPECT_FLOAT_EQ(state->wind_velocity()->x(), 0.0f);
    EXPECT_FLOAT_EQ(state->wind_velocity()->y(), 0.0f);
    EXPECT_FLOAT_EQ(state->wind_velocity()->z(), 0.0f);
}

TEST_F(InitialStateLoaderTest, ValidateLoadedTimeStepAndSimTime) {
    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, test_state_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_FLOAT_EQ(state->dt(), 0.01f);
    EXPECT_FLOAT_EQ(state->sim_time(), 0.0f);
}

TEST_F(InitialStateLoaderTest, LoadWithDifferentValues) {
    std::string custom_file = "custom_state.json";
    json custom_state = {
        {"sim_time", 5.0},
        {"dt", 0.02},
        {"position", {{"x", 100.0}, {"y", 200.0}, {"z", 5000.0}}},
        {"velocity", {{"x", 50.0}, {"y", -10.0}, {"z", 20.0}}},
        {"orientation", {{"x", 0.707f}, {"y", 0.0f}, {"z", 0.0f}, {"w", 0.707f}}},
        {"angular_velocity", {{"x", 0.1}, {"y", 0.2}, {"z", 0.3}}},
        {"atm_density", 0.5},
        {"atm_pressure", 50000.0},
        {"atm_temperature", 250.0},
        {"gravity", {{"x", 0.0f}, {"y", -9.5f}, {"z", 0.0f}}},
        {"wind_velocity", {{"x", 10.0f}, {"y", 5.0f}, {"z", 2.0f}}}
    };

    std::ofstream file(custom_file);
    file << custom_state.dump(4);
    file.close();

    flatbuffers::FlatBufferBuilder builder;
    ASSERT_TRUE(InitialStateLoader::create_state_from_json(builder, custom_file));

    auto state = state_vector::GetGeneralState(builder.GetBufferPointer());
    ASSERT_NE(state, nullptr);

    EXPECT_FLOAT_EQ(state->position()->x(), 100.0f);
    EXPECT_FLOAT_EQ(state->velocity()->y(), -10.0f);
    EXPECT_FLOAT_EQ(state->atm_density(), 0.5f);
    EXPECT_FLOAT_EQ(state->dt(), 0.02f);
    EXPECT_FLOAT_EQ(state->sim_time(), 5.0f);

    std::remove(custom_file.c_str());
}
