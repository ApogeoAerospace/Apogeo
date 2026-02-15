#include <gtest/gtest.h>
#include "PhysicsIntegrator.h"
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
  ASSERT_NE(integrator, nullptr);
  EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::EULER);
}

TEST_F(PhysicsIntegratorTest, SetIntegratorTypeEnum) {
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

TEST_F(PhysicsIntegratorTest, IntegrateAdvancesTimeOnlyWithTemporaryImpl) {
  PhysicsState initial;
  initial.position = Vector3(0, 0, 0);
  initial.velocity = Vector3(0, 0, 0);
  initial.angular_velocity = Vector3(0, 0, 0);
  initial.mass = 1.0;
  initial.time = 0.0;

  Vector3 force(10.0, 0.0, 0.0);
  Vector3 torque(0.0, 0.0, 0.0);
  double dt = 0.02;

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::EULER);
  PhysicsState after = integrator->integrate(initial, force, torque, dt);

  // Con la implementación temporal, solo avanza el tiempo.
  EXPECT_DOUBLE_EQ(after.time, initial.time + dt);
  EXPECT_DOUBLE_EQ(after.position.x, initial.position.x);
  EXPECT_DOUBLE_EQ(after.position.y, initial.position.y);
  EXPECT_DOUBLE_EQ(after.position.z, initial.position.z);
  EXPECT_DOUBLE_EQ(after.velocity.x, initial.velocity.x);
  EXPECT_DOUBLE_EQ(after.velocity.y, initial.velocity.y);
  EXPECT_DOUBLE_EQ(after.velocity.z, initial.velocity.z);
  EXPECT_DOUBLE_EQ(after.angular_velocity.x, initial.angular_velocity.x);
  EXPECT_DOUBLE_EQ(after.angular_velocity.y, initial.angular_velocity.y);
  EXPECT_DOUBLE_EQ(after.angular_velocity.z, initial.angular_velocity.z);
}

TEST_F(PhysicsIntegratorTest, MultipleIntegrationsAccumulateTimeOnly) {
  PhysicsState s;
  s.position = Vector3(0, 0, 0);
  s.velocity = Vector3(0, 0, 0);
  s.angular_velocity = Vector3(0, 0, 0);
  s.mass = 1.0;
  s.time = 0.0;

  Vector3 force(5.0, 0.0, 0.0);
  Vector3 torque(0.0, 0.0, 0.0);
  double dt = 0.01;

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

  for (int i = 0; i < 10; ++i) {
    s = integrator->integrate(s, force, torque, dt);
  }

  EXPECT_DOUBLE_EQ(s.time, 0.1);
  EXPECT_DOUBLE_EQ(s.position.x, 0.0);
  EXPECT_DOUBLE_EQ(s.velocity.x, 0.0);
}

TEST_F(PhysicsIntegratorTest, HandlesZeroMassGracefully) {
  PhysicsState initial;
  initial.position = Vector3(0, 0, 0);
  initial.velocity = Vector3(1.0, 0.0, 0.0);
  initial.angular_velocity = Vector3(0.0, 0.0, 0.0);
  initial.mass = 0.0;
  initial.time = 0.0;

  Vector3 force(10.0, 0.0, 0.0);
  Vector3 torque(0.0, 0.0, 0.0);
  double dt = 0.01;

  EXPECT_NO_THROW(integrator->integrate(initial, force, torque, dt));
}

TEST_F(PhysicsIntegratorTest, HandlesNegativeTimeStepGracefully) {
  PhysicsState initial;
  initial.position = Vector3(0, 0, 0);
  initial.velocity = Vector3(1.0, 0.0, 0.0);
  initial.angular_velocity = Vector3(0.0, 0.0, 0.0);
  initial.mass = 1.0;
  initial.time = 0.0;

  Vector3 force(10.0, 0.0, 0.0);
  Vector3 torque(0.0, 0.0, 0.0);
  double dt = -0.01;

  EXPECT_NO_THROW(integrator->integrate(initial, force, torque, dt));
}

TEST_F(PhysicsIntegratorTest, IntegrationPreservesTypeSetting) {
  PhysicsState s;
  s.position = Vector3(0, 0, 0);
  s.velocity = Vector3(0, 0, 0);
  s.angular_velocity = Vector3(0, 0, 0);
  s.mass = 1.0;
  s.time = 0.0;

  Vector3 force(10.0, 0.0, 0.0);
  Vector3 torque(0.0, 0.0, 0.0);

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::VERLET);
  EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::VERLET);

  s = integrator->integrate(s, force, torque, 0.01);

  EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::VERLET);
}

TEST_F(PhysicsIntegratorTest, FlatBufferFromGeneralStateConversion) {
  // Construir un GeneralState mínimo acorde al esquema nuevo.
  flatbuffers::FlatBufferBuilder builder;
  auto pos = state_vector::Vec3(1.0f, 2.0f, 3.0f);
  auto vel = state_vector::Vec3(4.0f, 5.0f, 6.0f);
  auto quat = state_vector::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
  auto ang = state_vector::Vec3(0.1f, 0.2f, 0.3f);

  state_vector::GeneralStateBuilder gs(builder);
  gs.add_sim_time(0.5f);
  gs.add_position(&pos);
  gs.add_velocity(&vel);
  gs.add_orientation(&quat);
  gs.add_angular_velocity(&ang);
  auto gs_off = gs.Finish();
  builder.Finish(gs_off);

  auto fb_state = state_vector::GetGeneralState(builder.GetBufferPointer());
  PhysicsState physics_state = PhysicsIntegrator::fromFlatBuffer(fb_state);

  EXPECT_DOUBLE_EQ(physics_state.position.x, 1.0);
  EXPECT_DOUBLE_EQ(physics_state.position.y, 2.0);
  EXPECT_DOUBLE_EQ(physics_state.position.z, 3.0);
  EXPECT_DOUBLE_EQ(physics_state.velocity.x, 4.0);
  EXPECT_DOUBLE_EQ(physics_state.velocity.y, 5.0);
  EXPECT_DOUBLE_EQ(physics_state.velocity.z, 6.0);
  EXPECT_DOUBLE_EQ(physics_state.time, 0.5);
}
