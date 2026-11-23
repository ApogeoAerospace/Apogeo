#include <gtest/gtest.h>
#include "structures_module.h"
#include <chrono>
#include <filesystem>
#include <fstream>

class StructuresModuleTest : public ::testing::Test {
protected:
    std::filesystem::path directory;
    structures::StructuresModule module;
    void SetUp() override {
        directory = std::filesystem::temp_directory_path() /
            ("apogeo-structures-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory));
    }
    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }
    std::string write(const std::string& name, const std::string& contents) {
        auto path = directory / name;
        std::ofstream file(path);
        file << contents;
        return path.string();
    }
};

TEST_F(StructuresModuleTest, LoadsMassAndInertiaAndRejectsMalformedFiles) {
    EXPECT_FALSE(module.loadMassPropertiesFromJson((directory / "missing.json").string()));
    EXPECT_FALSE(module.loadMassPropertiesFromJson(write("bad.json", "not json")));
    ASSERT_TRUE(module.loadMassPropertiesFromJson(write("mass.json", R"({
        "initial_total_mass_kg":1200,"fuel_mass_kg":100,
        "center_of_mass_m":{"x":1,"y":2,"z":3},
        "inertia_tensor_kg_m2":{"ixx":10,"iyy":20,"izz":30,"ixy":1,"ixz":2,"iyz":3}})")));
    EXPECT_DOUBLE_EQ(module.initialMassKg(), 1200);
    EXPECT_DOUBLE_EQ(module.getCenterOfMass().x, 1);
    EXPECT_DOUBLE_EQ(module.getCenterOfMass().y, 2);
    EXPECT_DOUBLE_EQ(module.getCenterOfMass().z, 3);
    auto inertia = module.getInertiaTensor();
    EXPECT_DOUBLE_EQ(inertia.ixx, 10);
    EXPECT_DOUBLE_EQ(inertia.iyy, 20);
    EXPECT_DOUBLE_EQ(inertia.izz, 30);
    EXPECT_DOUBLE_EQ(inertia.ixy, 1);
    EXPECT_DOUBLE_EQ(inertia.ixz, 2);
    EXPECT_DOUBLE_EQ(inertia.iyz, 3);
    EXPECT_FALSE(module.loadMassPropertiesFromJson(write("wrong.json", R"({"initial_total_mass_kg":"bad"})")));
}

TEST_F(StructuresModuleTest, LoadsAllLimitsAndSkipsInvalidRows) {
    EXPECT_FALSE(module.loadStructuralLimitsFromCsv((directory / "missing.csv").string()));
    ASSERT_TRUE(module.loadStructuralLimitsFromCsv(write("limits.csv",
        "limit,value\n\ninvalid\nmax_g_load,bad\nmax_g_load,8\nwarning_g_load,6\n"
        "max_dynamic_pressure,60000\nwarning_dynamic_pressure,40000\nunknown,1\n")));
    EXPECT_DOUBLE_EQ(module.maxGLoad(), 8);
    EXPECT_DOUBLE_EQ(module.warningGLoad(), 6);
    EXPECT_TRUE(module.checkStructuralIntegrity(60000, 8));
    EXPECT_FALSE(module.checkStructuralIntegrity(60001, 8));
    EXPECT_FALSE(module.checkStructuralIntegrity(60000, 9));
}

TEST_F(StructuresModuleTest, ComputesDampingAndHandlesAbsentStateFields) {
    EXPECT_FLOAT_EQ(module.computeStructuralForce(nullptr).x, 0);
    EXPECT_FLOAT_EQ(module.computeStructuralTorque(nullptr).z, 0);
    flatbuffers::FlatBufferBuilder empty;
    empty.Finish(state_vector::CreateGeneralState(empty));
    auto* missing = state_vector::GetGeneralState(empty.GetBufferPointer());
    EXPECT_FLOAT_EQ(module.computeStructuralForce(missing).x, 0);
    EXPECT_FLOAT_EQ(module.computeStructuralTorque(missing).x, 0);
    flatbuffers::FlatBufferBuilder builder;
    state_vector::Vec3 velocity(1, -2, 3), angular(2, -3, 4);
    state_vector::GeneralStateBuilder state(builder);
    state.add_velocity(&velocity);
    state.add_angular_velocity(&angular);
    builder.Finish(state.Finish());
    auto* input = state_vector::GetGeneralState(builder.GetBufferPointer());
    auto force = module.computeStructuralForce(input);
    auto torque = module.computeStructuralTorque(input);
    EXPECT_FLOAT_EQ(force.x, -5);
    EXPECT_FLOAT_EQ(force.y, 10);
    EXPECT_FLOAT_EQ(force.z, -15);
    EXPECT_FLOAT_EQ(torque.x, -6);
    EXPECT_FLOAT_EQ(torque.y, 9);
    EXPECT_FLOAT_EQ(torque.z, -12);
}
