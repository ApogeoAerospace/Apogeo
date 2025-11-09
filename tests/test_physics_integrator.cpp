#include <gtest/gtest.h>
#include "PhysicsIntegrator.h"
#include <cmath>

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
    // Posición: p = 0.5*a*t² = 0.5*10*0.01² = 0.0005
    EXPECT_NEAR(new_state.position.y, 0.0005, 0.0001);
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
