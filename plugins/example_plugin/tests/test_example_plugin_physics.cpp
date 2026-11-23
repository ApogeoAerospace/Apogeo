#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>

#include "example_physics.h"

namespace ex = example_plugin;

namespace {

ex::TickInput makeInput() {
    ex::TickInput in;
    in.mass = 1000.0;
    in.gravity = {0.0, 0.0, -9.81};
    in.velocity = {100.0, 0.0, 0.0};
    in.wind = {0.0, 0.0, 0.0};
    in.air_density = 1.225;
    return in;
}

} // namespace

TEST(ExamplePluginPhysicsTest, DefaultParametersAreInertAndValid) {
    const ex::Params params;
    EXPECT_TRUE(ex::validate(params));

    ex::Forces forces;
    ASSERT_TRUE(ex::computeForces(params, makeInput(), forces));
    EXPECT_DOUBLE_EQ(forces.total.x, 0.0);
    EXPECT_DOUBLE_EQ(forces.total.y, 0.0);
    EXPECT_DOUBLE_EQ(forces.total.z, 0.0);
}

TEST(ExamplePluginPhysicsTest, GravityForceIsMassTimesGravityVector) {
    ex::Params params;
    params.apply_gravity = true;

    ex::Forces forces;
    ASSERT_TRUE(ex::computeForces(params, makeInput(), forces));
    EXPECT_NEAR(forces.gravity.x, 0.0, 1e-9);
    EXPECT_NEAR(forces.gravity.z, -9810.0, 1e-9);
    EXPECT_NEAR(forces.total.z, -9810.0, 1e-9);
}

TEST(ExamplePluginPhysicsTest, GravityFollowsTheVectorStoredInTheState) {
    ex::Params params;
    params.apply_gravity = true;

    // A planet with different gravity and a sideways direction, as set from the web panel.
    ex::TickInput in = makeInput();
    in.gravity = {0.0, -3.7, 0.0};

    ex::Forces forces;
    ASSERT_TRUE(ex::computeForces(params, in, forces));
    EXPECT_NEAR(forces.gravity.y, -3700.0, 1e-9);
    EXPECT_NEAR(forces.gravity.z, 0.0, 1e-12);
}

TEST(ExamplePluginPhysicsTest, DisabledGravityVectorProducesNoForce) {
    ex::Params params;
    params.apply_gravity = true;

    ex::TickInput in = makeInput();
    in.gravity = {0.0, 0.0, 0.0};  // the web panel seeds a zero vector when gravity is off

    ex::Forces forces;
    ASSERT_TRUE(ex::computeForces(params, in, forces));
    EXPECT_DOUBLE_EQ(forces.total.z, 0.0);
}

TEST(ExamplePluginPhysicsTest, GravityScaleMultipliesTheForce) {
    ex::Params params;
    params.apply_gravity = true;
    params.gravity_scale = 0.5;

    ex::Forces forces;
    ASSERT_TRUE(ex::computeForces(params, makeInput(), forces));
    EXPECT_NEAR(forces.gravity.z, -4905.0, 1e-9);
}

TEST(ExamplePluginPhysicsTest, GravityNeedsPositiveMass) {
    ex::Params params;
    params.apply_gravity = true;

    ex::TickInput in = makeInput();
    in.mass = 0.0;

    ex::Forces forces;
    std::string error;
    EXPECT_FALSE(ex::computeForces(params, in, forces, &error));
    EXPECT_FALSE(error.empty());
    EXPECT_DOUBLE_EQ(forces.total.z, 0.0);
}

TEST(ExamplePluginPhysicsTest, DragOpposesVelocityWithQuadraticMagnitude) {
    ex::Params params;
    params.drag_coefficient = 0.5;
    params.reference_area = 2.0;

    ex::Forces forces;
    ASSERT_TRUE(ex::computeForces(params, makeInput(), forces));

    // 0.5 * rho * Cd * A * v^2 = 0.5 * 1.225 * 0.5 * 2 * 100^2 = 6125 N
    EXPECT_NEAR(forces.drag.x, -6125.0, 1e-9);
    EXPECT_NEAR(forces.drag.y, 0.0, 1e-12);
    EXPECT_NEAR(forces.drag.z, 0.0, 1e-12);
}

TEST(ExamplePluginPhysicsTest, DragUsesVelocityRelativeToTheWind) {
    ex::Params params;
    params.drag_coefficient = 0.5;

    ex::TickInput in = makeInput();
    in.wind = in.velocity;  // moving with the air mass: no relative flow

    ex::Forces forces;
    ASSERT_TRUE(ex::computeForces(params, in, forces));
    EXPECT_NEAR(ex::norm(forces.drag), 0.0, 1e-12);
}

TEST(ExamplePluginPhysicsTest, TotalIsTheSumOfGravityAndDrag) {
    ex::Params params;
    params.apply_gravity = true;
    params.drag_coefficient = 0.5;

    ex::TickInput in = makeInput();
    in.velocity = {30.0, 0.0, 40.0};

    ex::Forces forces;
    ASSERT_TRUE(ex::computeForces(params, in, forces));
    EXPECT_NEAR(forces.total.x, forces.gravity.x + forces.drag.x, 1e-9);
    EXPECT_NEAR(forces.total.z, forces.gravity.z + forces.drag.z, 1e-9);
    EXPECT_LT(forces.drag.z, 0.0);  // moving up: drag pulls down
}

TEST(ExamplePluginPhysicsTest, NonFiniteStateIsRejected) {
    ex::Params params;
    params.apply_gravity = true;

    ex::TickInput in = makeInput();
    in.velocity.x = std::numeric_limits<double>::quiet_NaN();

    ex::Forces forces;
    std::string error;
    EXPECT_FALSE(ex::computeForces(params, in, forces, &error));
    EXPECT_NE(error.find("non finite"), std::string::npos);
    EXPECT_DOUBLE_EQ(forces.total.z, 0.0);
}

TEST(ExamplePluginPhysicsTest, ValidationRejectsNonsenseParameters) {
    std::string error;

    ex::Params params;
    params.log_interval_ticks = 0;
    EXPECT_FALSE(ex::validate(params, &error));

    params = ex::Params{};
    params.drag_coefficient = -0.1;
    EXPECT_FALSE(ex::validate(params, &error));

    params = ex::Params{};
    params.reference_area = 0.0;
    EXPECT_FALSE(ex::validate(params, &error));

    params = ex::Params{};
    params.gravity_scale = std::numeric_limits<double>::infinity();
    EXPECT_FALSE(ex::validate(params, &error));

    params = ex::Params{};
    params.gravity_scale = -1.0;
    EXPECT_FALSE(ex::validate(params, &error));

    params = ex::Params{};
    params.step_size = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(ex::validate(params, &error));
}
