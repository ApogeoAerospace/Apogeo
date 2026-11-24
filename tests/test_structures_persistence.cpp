#include <gtest/gtest.h>
#include "structures_module.h"
#include <nlohmann/json.hpp>

/**
 * @file test_structures_persistence.cpp
 * @brief Unit tests for Structures plugin state persistence and schema validation.
 */

using json = nlohmann::json;

TEST(StructuresPersistenceTest, MutableStateRoundtrip) {
    structures::StructuresModule mod;
    mod.setFuelMassKg(450.5);
    mod.setCenterOfMass({1.0, 2.0, 3.0});
    mod.setInertiaTensor({100.0, 200.0, 300.0, 1.0, 2.0, 3.0});

    std::string saved_json = mod.saveState();
    EXPECT_FALSE(saved_json.empty());

    structures::StructuresModule restored_mod;
    EXPECT_TRUE(restored_mod.loadState(saved_json));
    EXPECT_DOUBLE_EQ(restored_mod.fuelMassKg(), 450.5);
    EXPECT_DOUBLE_EQ(restored_mod.getCenterOfMass().x, 1.0);
    EXPECT_DOUBLE_EQ(restored_mod.getCenterOfMass().y, 2.0);
    EXPECT_DOUBLE_EQ(restored_mod.getCenterOfMass().z, 3.0);
    EXPECT_DOUBLE_EQ(restored_mod.getInertiaTensor().ixx, 100.0);
    EXPECT_DOUBLE_EQ(restored_mod.getInertiaTensor().iyy, 200.0);
    EXPECT_DOUBLE_EQ(restored_mod.getInertiaTensor().izz, 300.0);
}

TEST(StructuresPersistenceTest, IncompatibleSchemaRejected) {
    structures::StructuresModule mod;
    json bad_schema = {
        {"schema_version", 999}, // Unsupported version
        {"fuel_mass_kg", 500.0}
    };
    EXPECT_FALSE(mod.loadState(bad_schema.dump()));
}

