#include <gtest/gtest.h>
#include "PhysicsIntegrator.h"
#include <cmath>
#include <flatbuffers/flatbuffers.h>
#include "state_vector_generated.h"

using namespace MoLab;

class PhysicsIntegratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        integrator = new PhysicsIntegrator(PhysicsIntegrator::IntegratorType::EULER);
    }

    void TearDown() override {
        delete integrator;
    }

    PhysicsIntegrator* integrator;
};

TEST_F(PhysicsIntegratorTest, CreateIntegrator) {
    EXPECT_NE(integrator, nullptr);
    EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::EULER);
}

TEST_F(PhysicsIntegratorTest, SetIntegratorType) {
    integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);
    EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

    integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::VERLET);
    EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::VERLET);
}

TEST_F(PhysicsIntegratorTest, SetIntegratorTypeByName) {
    integrator->setIntegratorType("euler");
    EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::EULER);

    integrator->setIntegratorType("runge_kutta_4");
    EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

    integrator->setIntegratorType("verlet");
    EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::VERLET);
}

TEST_F(PhysicsIntegratorTest, GetIntegratorTypeName) {
    integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::EULER);
    EXPECT_EQ(integrator->getIntegratorTypeName(), "euler");

    integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);
    EXPECT_EQ(integrator->getIntegratorTypeName(), "runge_kutta_4");
}

TEST_F(PhysicsIntegratorTest, EulerIntegrationWithConstantForce) {
    // Estado inicial: objeto en reposo
    PhysicsState initial_state;
    initial_state.position = Vector3(0, 0, 0);
    initial_state.velocity = Vector3(0, 0, 0);
    initial_state.mass = 1.0;

    // Fuerza constante hacia arriba
    Vector3 force(0, 10.0, 0);  // 10 N hacia arriba
    Vector3 torque(0, 0, 0);
    double dt = 0.01;

    integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::EULER);
    PhysicsState new_state = integrator->integrate(initial_state, force, torque, dt);

    // Con F=ma, a=10 m/s², después de 0.01s: v = 0.1 m/s
    EXPECT_NEAR(new_state.velocity.y, 0.1, 0.001);
    // Posición con Euler semi-implícito: p = p0 + v_new*dt = 0 + 0.1*0.01 = 0.001
    EXPECT_NEAR(new_state.position.y, 0.001, 0.0001);
}

TEST_F(PhysicsIntegratorTest, IntegrationWithZeroForce) {
    // Estado inicial con velocidad constante
    PhysicsState initial_state;
    initial_state.position = Vector3(0, 0, 0);
    initial_state.velocity = Vector3(10, 0, 0);  // 10 m/s en X
    initial_state.mass = 1.0;

    Vector3 force(0, 0, 0);  // Sin fuerza
    Vector3 torque(0, 0, 0);
    double dt = 0.1;

    PhysicsState new_state = integrator->integrate(initial_state, force, torque, dt);

    // Velocidad debe permanecer constante
    EXPECT_DOUBLE_EQ(new_state.velocity.x, 10.0);
    // Posición debe avanzar: 10 * 0.1 = 1.0
    EXPECT_NEAR(new_state.position.x, 1.0, 0.001);
}

TEST_F(PhysicsIntegratorTest, Vector3Operations) {
    Vector3 v1(1.0, 2.0, 3.0);
    Vector3 v2(4.0, 5.0, 6.0);

    // Suma
    Vector3 sum = v1 + v2;
    EXPECT_DOUBLE_EQ(sum.x, 5.0);
    EXPECT_DOUBLE_EQ(sum.y, 7.0);
    EXPECT_DOUBLE_EQ(sum.z, 9.0);

    // Resta
    Vector3 diff = v2 - v1;
    EXPECT_DOUBLE_EQ(diff.x, 3.0);
    EXPECT_DOUBLE_EQ(diff.y, 3.0);
    EXPECT_DOUBLE_EQ(diff.z, 3.0);

    // Multiplicación escalar
    Vector3 scaled = v1 * 2.0;
    EXPECT_DOUBLE_EQ(scaled.x, 2.0);
    EXPECT_DOUBLE_EQ(scaled.y, 4.0);
    EXPECT_DOUBLE_EQ(scaled.z, 6.0);
}

TEST_F(PhysicsIntegratorTest, Vector3Magnitude) {
    Vector3 v(3.0, 4.0, 0.0);
    EXPECT_DOUBLE_EQ(v.magnitude(), 5.0);

    Vector3 v2(1.0, 1.0, 1.0);
    EXPECT_NEAR(v2.magnitude(), std::sqrt(3.0), 0.0001);
}

TEST_F(PhysicsIntegratorTest, Vector3Normalized) {
    Vector3 v(3.0, 4.0, 0.0);
    Vector3 normalized = v.normalized();

    EXPECT_NEAR(normalized.magnitude(), 1.0, 0.0001);
    EXPECT_NEAR(normalized.x, 0.6, 0.0001);
    EXPECT_NEAR(normalized.y, 0.8, 0.0001);
}

TEST_F(PhysicsIntegratorTest, RungeKutta4Integration) {
    PhysicsState initial_state;
    initial_state.position = Vector3(0, 0, 0);
    initial_state.velocity = Vector3(0, 0, 0);
    initial_state.mass = 1.0;

    Vector3 force(0, 10.0, 0);
    Vector3 torque(0, 0, 0);
    double dt = 0.01;

    integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);
    PhysicsState new_state = integrator->integrate(initial_state, force, torque, dt);

    EXPECT_GT(new_state.velocity.y, 0.0);
    EXPECT_GT(new_state.position.y, 0.0);
}

TEST_F(PhysicsIntegratorTest, VerletIntegration) {
    PhysicsState initial_state;
    initial_state.position = Vector3(0, 0, 0);
    initial_state.velocity = Vector3(0, 0, 0);
    initial_state.mass = 1.0;

    Vector3 force(0, 10.0, 0);
    Vector3 torque(0, 0, 0);
    double dt = 0.01;

    integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::VERLET);
    PhysicsState new_state = integrator->integrate(initial_state, force, torque, dt);

    EXPECT_GT(new_state.velocity.y, 0.0);
    EXPECT_GT(new_state.position.y, 0.0);
}

TEST_F(PhysicsIntegratorTest, IntegrationWithMass) {
    PhysicsState state1;
    state1.position = Vector3(0, 0, 0);
    state1.velocity = Vector3(0, 0, 0);
    state1.mass = 1.0;

    PhysicsState state2;
    state2.position = Vector3(0, 0, 0);
    state2.velocity = Vector3(0, 0, 0);
    state2.mass = 10.0;

    Vector3 force(10.0, 0, 0);
    Vector3 torque(0, 0, 0);
    double dt = 0.01;

    PhysicsState new_state1 = integrator->integrate(state1, force, torque, dt);
    PhysicsState new_state2 = integrator->integrate(state2, force, torque, dt);

    // Heavier object should accelerate less
    EXPECT_GT(new_state1.velocity.x, new_state2.velocity.x);
}

TEST_F(PhysicsIntegratorTest, InvalidIntegratorTypeName) {
    integrator->setIntegratorType("invalid_type");
    // Should default to EULER or remain unchanged
    EXPECT_NO_THROW(integrator->getIntegratorTypeName());
}

TEST_F(PhysicsIntegratorTest, LargeTimeStep) {
    PhysicsState initial_state;
    initial_state.position = Vector3(0, 0, 0);
    initial_state.velocity = Vector3(0, 0, 0);
    initial_state.mass = 1.0;

    Vector3 force(10.0, 0, 0);
    Vector3 torque(0, 0, 0);
    double dt = 1.0;  // Large time step

    EXPECT_NO_THROW(integrator->integrate(initial_state, force, torque, dt));
}

TEST_F(PhysicsIntegratorTest, ZeroMass) {
    PhysicsState initial_state;
    initial_state.position = Vector3(0, 0, 0);
    initial_state.velocity = Vector3(10, 0, 0);
    initial_state.mass = 0.0;  // Zero mass

    Vector3 force(10.0, 0, 0);
    Vector3 torque(0, 0, 0);
    double dt = 0.01;

    // Should handle gracefully (avoid division by zero)
    EXPECT_NO_THROW(integrator->integrate(initial_state, force, torque, dt));
}

TEST_F(PhysicsIntegratorTest, NegativeTimeStep) {
    PhysicsState initial_state;
    initial_state.position = Vector3(0, 0, 0);
    initial_state.velocity = Vector3(10, 0, 0);
    initial_state.mass = 1.0;

    Vector3 force(10.0, 0, 0);
    Vector3 torque(0, 0, 0);
    double dt = -0.01;  // Negative time step

    EXPECT_NO_THROW(integrator->integrate(initial_state, force, torque, dt));
}

TEST_F(PhysicsIntegratorTest, MultipleIntegrations) {
    PhysicsState state;
    state.position = Vector3(0, 0, 0);
    state.velocity = Vector3(0, 0, 0);
    state.mass = 1.0;

    Vector3 force(10.0, 0, 0);
    Vector3 torque(0, 0, 0);
    double dt = 0.01;

    // Integrate multiple times
    for (int i = 0; i < 10; i++) {
        state = integrator->integrate(state, force, torque, dt);
    }

    EXPECT_GT(state.velocity.x, 0.0);
    EXPECT_GT(state.position.x, 0.0);
}

TEST_F(PhysicsIntegratorTest, Vector3DefaultConstructor) {
    Vector3 v;
    EXPECT_DOUBLE_EQ(v.x, 0.0);
    EXPECT_DOUBLE_EQ(v.y, 0.0);
    EXPECT_DOUBLE_EQ(v.z, 0.0);
}

TEST_F(PhysicsIntegratorTest, Vector3AddAssign) {
    Vector3 v1(1.0, 2.0, 3.0);
    Vector3 v2(4.0, 5.0, 6.0);
    v1 += v2;
    
    EXPECT_DOUBLE_EQ(v1.x, 5.0);
    EXPECT_DOUBLE_EQ(v1.y, 7.0);
    EXPECT_DOUBLE_EQ(v1.z, 9.0);
}

TEST_F(PhysicsIntegratorTest, Vector3NormalizeZeroVector) {
    Vector3 v(0.0, 0.0, 0.0);
    Vector3 normalized = v.normalized();
    
    EXPECT_DOUBLE_EQ(normalized.x, 0.0);
    EXPECT_DOUBLE_EQ(normalized.y, 0.0);
    EXPECT_DOUBLE_EQ(normalized.z, 0.0);
}

TEST_F(PhysicsIntegratorTest, AtmosphericDragCalculation) {
    Vector3 velocity(100.0, 0.0, 0.0);
    double air_density = 1.225;
    double drag_coefficient = 0.5;
    double reference_area = 1.0;
    
    Vector3 drag = AtmosphericEffects::calculateDrag(
        velocity, air_density, drag_coefficient, reference_area
    );
    
    // Drag should oppose velocity
    EXPECT_LT(drag.x, 0.0);
}

TEST_F(PhysicsIntegratorTest, AirDensityAtSeaLevel) {
    double density = AtmosphericEffects::calculateAirDensity(0.0);
    EXPECT_GT(density, 1.0);
    EXPECT_LT(density, 1.5);
}

TEST_F(PhysicsIntegratorTest, AirDensityAtAltitude) {
    double density_low = AtmosphericEffects::calculateAirDensity(0.0);
    double density_high = AtmosphericEffects::calculateAirDensity(10000.0);
    
    // Density decreases with altitude
    EXPECT_LT(density_high, density_low);
}

TEST_F(PhysicsIntegratorTest, EarthGravityCalculation) {
    Vector3 position(0.0, 0.0, 1000.0);
    Vector3 gravity = GravitationalEffects::calculateEarthGravity(position);
    
    // Gravity should point downward (negative z or magnitude check)
    EXPECT_GT(gravity.magnitude(), 0.0);
}

TEST_F(PhysicsIntegratorTest, CentralGravityCalculation) {
    Vector3 position(1000.0, 0.0, 0.0);
    double central_mass = 1e24;
    
    Vector3 gravity = GravitationalEffects::calculateCentralGravity(
        position, central_mass
    );
    
    EXPECT_GT(gravity.magnitude(), 0.0);
}

TEST_F(PhysicsIntegratorTest, WindEffectCalculation) {
    Vector3 wind_velocity(10.0, 0.0, 0.0);
    Vector3 object_velocity(5.0, 0.0, 0.0);
    double air_density = 1.225;
    double reference_area = 1.0;
    
    Vector3 wind_force = AtmosphericEffects::calculateWind(
        wind_velocity, object_velocity, air_density, reference_area
    );
    
    EXPECT_NO_THROW(wind_force.magnitude());
}

TEST_F(PhysicsIntegratorTest, FlatBufferConversion) {
    flatbuffers::FlatBufferBuilder builder;
    
    auto pos = state_vector::Vec3(1.0, 2.0, 3.0);
    auto vel = state_vector::Vec3(4.0, 5.0, 6.0);
    auto ori = state_vector::Quaternion(0, 0, 0, 1);
    auto grav = state_vector::Vec3(0, -9.81, 0);
    auto wind = state_vector::Vec3(0, 0, 0);
    
    auto state = state_vector::CreateGeneralState(
        builder, &pos, &vel, &ori, 1.225, 101325, 288.15, &grav, 0.0, 0.0, &wind
    );
    builder.Finish(state);
    
    auto fb_state = state_vector::GetGeneralState(builder.GetBufferPointer());
    PhysicsState physics_state = PhysicsIntegrator::fromFlatBuffer(fb_state);
    
    EXPECT_DOUBLE_EQ(physics_state.position.x, 1.0);
    EXPECT_DOUBLE_EQ(physics_state.position.y, 2.0);
    EXPECT_DOUBLE_EQ(physics_state.position.z, 3.0);
    EXPECT_DOUBLE_EQ(physics_state.velocity.x, 4.0);
    EXPECT_DOUBLE_EQ(physics_state.velocity.y, 5.0);
    EXPECT_DOUBLE_EQ(physics_state.velocity.z, 6.0);
}

TEST_F(PhysicsIntegratorTest, ToFlatBufferConversion) {
    PhysicsState physics_state;
    physics_state.position = Vector3(10.0, 20.0, 30.0);
    physics_state.velocity = Vector3(1.0, 2.0, 3.0);
    physics_state.mass = 1500.0;
    
    flatbuffers::FlatBufferBuilder builder;
    PhysicsIntegrator::toFlatBuffer(builder, physics_state);
    
    EXPECT_GT(builder.GetSize(), 0u);
}

TEST_F(PhysicsIntegratorTest, PhysicsStateDefaultConstructor) {
    PhysicsState state;
    EXPECT_DOUBLE_EQ(state.mass, 1.0);
    EXPECT_DOUBLE_EQ(state.time, 0.0);
}

TEST_F(PhysicsIntegratorTest, IntegrationPreservesType) {
    PhysicsState state;
    state.position = Vector3(0, 0, 0);
    state.velocity = Vector3(0, 0, 0);
    state.mass = 1.0;
    
    Vector3 force(10, 0, 0);
    Vector3 torque(0, 0, 0);
    
    integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);
    EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);
    
    integrator->integrate(state, force, torque, 0.01);
    
    EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);
}
