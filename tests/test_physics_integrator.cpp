#include <gtest/gtest.h>
#include "PhysicsIntegrator.h"
#include <flatbuffers/flatbuffers.h>
#include "state_vector_generated.h"
#include <cmath>

/**
 * @file test_physics_integrator.cpp
 * @brief Pruebas unitarias para `PhysicsIntegrator`.
 */

using namespace MoLab;

// ---------------------------------------------------------------------------
// Fixture base
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Tests de API y configuración
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Tests básicos de integración lineal
// ---------------------------------------------------------------------------

// Euler explícito, 1 paso: F=10 N, m=1 kg, dt=0.02 s
//   v_new = 0 + 10*0.02 = 0.2 m/s
//   x_new = 0 + 0*0.02  = 0.0 m  (Euler usa velocidad del paso actual)
TEST_F(PhysicsIntegratorTest, EulerConstantForceOneStep) {
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

  EXPECT_DOUBLE_EQ(after.time, 0.02);
  EXPECT_DOUBLE_EQ(after.position.x(), 0.0);
  EXPECT_NEAR(after.velocity.x(), 0.2, 1e-12);
}

// RK4, aceleración constante a=10 m/s², 100 pasos de dt=0.01 => t=1.0 s
//   x(1) = 0.5*10*1² = 5.0 m,  v(1) = 10*1 = 10.0 m/s
TEST_F(PhysicsIntegratorTest, RK4ConstantAccelerationConvergence) {
  PhysicsState s;
  s.mass = 1.0;

  Vector3 force(10.0, 0.0, 0.0);
  Vector3 torque(0.0, 0.0, 0.0);
  double dt = 0.01;

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

  for (int i = 0; i < 100; ++i) {
    s = integrator->integrate(s, force, torque, dt);
  }

  EXPECT_NEAR(s.time, 1.0, 1e-10);
  EXPECT_NEAR(s.velocity.x(), 10.0, 1e-8);
  EXPECT_NEAR(s.position.x(), 5.0, 1e-8);
}

// Velocity-Verlet, misma prueba de aceleración constante
TEST_F(PhysicsIntegratorTest, VerletConstantAccelerationConvergence) {
  PhysicsState s;
  s.mass = 1.0;

  Vector3 force(10.0, 0.0, 0.0);
  Vector3 torque(0.0, 0.0, 0.0);
  double dt = 0.01;

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::VERLET);

  for (int i = 0; i < 100; ++i) {
    s = integrator->integrate(s, force, torque, dt);
  }

  EXPECT_NEAR(s.time, 1.0, 1e-10);
  EXPECT_NEAR(s.velocity.x(), 10.0, 1e-8);
  EXPECT_NEAR(s.position.x(), 5.0, 1e-8);
}

// ---------------------------------------------------------------------------
// Tests de robustez
// ---------------------------------------------------------------------------

TEST_F(PhysicsIntegratorTest, HandlesZeroMassGracefully) {
  PhysicsState initial;
  initial.velocity = Vector3(1.0, 0.0, 0.0);
  initial.mass = 0.0;

  EXPECT_NO_THROW(integrator->integrate(initial, Vector3(10, 0, 0), Vector3::Zero(), 0.01));
}

TEST_F(PhysicsIntegratorTest, HandlesNegativeTimeStepGracefully) {
  PhysicsState initial;
  initial.velocity = Vector3(1.0, 0.0, 0.0);
  initial.mass = 1.0;

  EXPECT_NO_THROW(integrator->integrate(initial, Vector3(10, 0, 0), Vector3::Zero(), -0.01));
}

TEST_F(PhysicsIntegratorTest, IntegrationPreservesTypeSetting) {
  PhysicsState s;
  s.mass = 1.0;

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::VERLET);
  s = integrator->integrate(s, Vector3(10, 0, 0), Vector3::Zero(), 0.01);
  EXPECT_EQ(integrator->getIntegratorType(), PhysicsIntegrator::IntegratorType::VERLET);
}

// ---------------------------------------------------------------------------
// Test: Tiro parabólico bajo gravedad constante (RK4)
// ---------------------------------------------------------------------------
// Proyectil lanzado con v0 = (100, 0, 100) m/s, gravedad g = -9.81 m/s² en Z.
// Solución analítica:
//   x(t) = v0x * t                      = 100 * 5 = 500 m
//   z(t) = v0z * t + 0.5 * (-g) * t²   = 100*5 - 0.5*9.81*25 = 377.375 m
//   vz(t) = v0z + (-g) * t              = 100 - 9.81*5 = 50.95 m/s
TEST_F(PhysicsIntegratorTest, ProjectileMotionUnderGravity) {
  PhysicsState s;
  s.position = Vector3(0, 0, 0);
  s.velocity = Vector3(100.0, 0.0, 100.0);
  s.mass = 10.0;       // masa arbitraria, no afecta cinemática (a = F/m)

  // F = m * g, con g = (0, 0, -9.81)
  Vector3 gravity_force = Vector3(0.0, 0.0, -9.81) * s.mass;
  Vector3 torque = Vector3::Zero();
  double dt = 0.001;    // paso pequeño para precisión
  double t_final = 5.0;
  int steps = static_cast<int>(t_final / dt);

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

  for (int i = 0; i < steps; ++i) {
    s = integrator->integrate(s, gravity_force, torque, dt);
  }

  // Solución analítica a t=5 s
  double expected_x  = 100.0 * t_final;
  double expected_z  = 100.0 * t_final + 0.5 * (-9.81) * t_final * t_final;
  double expected_vz = 100.0 + (-9.81) * t_final;

  EXPECT_NEAR(s.position.x(), expected_x,  1e-4);
  EXPECT_NEAR(s.position.z(), expected_z,  1e-4);
  EXPECT_NEAR(s.velocity.x(), 100.0,       1e-6);
  EXPECT_NEAR(s.velocity.z(), expected_vz, 1e-4);
  EXPECT_NEAR(s.position.y(), 0.0,         1e-10);
}

// ---------------------------------------------------------------------------
// Test: Caída libre multieje (todos los integradores)
// ---------------------------------------------------------------------------
// m = 500 kg, F = (0, -4905, 0) N => a = (0, -9.81, 0) m/s²
// v0 = (0, 0, 0), t = 10 s
//   y(10) = -0.5 * 9.81 * 100 = -490.5 m
//   vy(10) = -9.81 * 10 = -98.1 m/s
class FreeFallTest : public ::testing::TestWithParam<PhysicsIntegrator::IntegratorType> {};

TEST_P(FreeFallTest, FreeFallAnalyticalMatch) {
  PhysicsIntegrator integrator(GetParam());

  PhysicsState s;
  s.mass = 500.0;

  Vector3 weight(0.0, -9.81 * s.mass, 0.0);
  Vector3 torque = Vector3::Zero();

  double dt = 0.001;
  double t_final = 10.0;
  int steps = static_cast<int>(t_final / dt);

  for (int i = 0; i < steps; ++i) {
    s = integrator.integrate(s, weight, torque, dt);
  }

  double expected_y  = 0.5 * (-9.81) * t_final * t_final;
  double expected_vy = -9.81 * t_final;

  // Euler tiene mayor error acumulado, tolerancias ajustadas por método
  double tol_pos = (GetParam() == PhysicsIntegrator::IntegratorType::EULER) ? 0.06 : 1e-4;
  double tol_vel = (GetParam() == PhysicsIntegrator::IntegratorType::EULER) ? 1e-6 : 1e-6;

  EXPECT_NEAR(s.position.y(), expected_y,  tol_pos);
  EXPECT_NEAR(s.velocity.y(), expected_vy, tol_vel);
  EXPECT_NEAR(s.position.x(), 0.0, 1e-10);
  EXPECT_NEAR(s.position.z(), 0.0, 1e-10);
}

INSTANTIATE_TEST_SUITE_P(
    AllIntegrators,
    FreeFallTest,
    ::testing::Values(
        PhysicsIntegrator::IntegratorType::EULER,
        PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4,
        PhysicsIntegrator::IntegratorType::VERLET
    )
);

// ---------------------------------------------------------------------------
// Test: Convergencia de orden — RK4 vs Euler
// ---------------------------------------------------------------------------
// Dado el mismo problema (caída libre, t=1 s), verificar que RK4 con
// paso grande es más preciso que Euler con el mismo paso.
TEST_F(PhysicsIntegratorTest, RK4MoreAccurateThanEuler) {
  auto run_freefall = [](PhysicsIntegrator::IntegratorType type, double dt) {
    PhysicsIntegrator integ(type);
    PhysicsState s;
    s.mass = 1.0;

    Vector3 weight(0.0, -9.81, 0.0);
    int steps = static_cast<int>(1.0 / dt);

    for (int i = 0; i < steps; ++i) {
      s = integ.integrate(s, weight, Vector3::Zero(), dt);
    }
    return s;
  };

  double dt = 0.05;     // paso relativamente grande
  double expected_y = 0.5 * (-9.81) * 1.0;

  PhysicsState euler_result = run_freefall(PhysicsIntegrator::IntegratorType::EULER, dt);
  PhysicsState rk4_result   = run_freefall(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4, dt);

  double euler_err = std::abs(euler_result.position.y() - expected_y);
  double rk4_err   = std::abs(rk4_result.position.y()   - expected_y);

  // RK4 debe ser órdenes de magnitud más preciso
  EXPECT_LT(rk4_err, euler_err * 0.01);
}

// ---------------------------------------------------------------------------
// Test: Rotación libre de cuerpo rígido simétrico
// ---------------------------------------------------------------------------
// Un cuerpo con inercia diagonal Ixx = Iyy = 10, Izz = 5 y omega_0 = (0, 0, 10) rad/s
// (rotación pura alrededor del eje principal Z).
// Sin torque externo:
//   - omega debe permanecer constante (no hay término giroscópico en eje principal)
//   - La energía cinética rotacional debe conservarse: T = 0.5 * Izz * wz²
TEST_F(PhysicsIntegratorTest, TorqueFreeSymmetricBodySpinStability) {
  PhysicsState s;
  s.mass = 50.0;
  s.inertia = makeInertiaTensor(10.0, 10.0, 5.0, 0.0, 0.0, 0.0);
  s.angular_velocity = Vector3(0.0, 0.0, 10.0);

  Vector3 force = Vector3::Zero();
  Vector3 torque = Vector3::Zero();
  double dt = 0.001;

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

  double initial_energy = 0.5 * 5.0 * 10.0 * 10.0;    // 0.5 * Izz * wz²

  for (int i = 0; i < 10000; ++i) {     // 10 s de simulación
    s = integrator->integrate(s, force, torque, dt);
  }

  // Velocidad angular debe mantenerse constante
  EXPECT_NEAR(s.angular_velocity.x(), 0.0,  1e-6);
  EXPECT_NEAR(s.angular_velocity.y(), 0.0,  1e-6);
  EXPECT_NEAR(s.angular_velocity.z(), 10.0, 1e-6);

  // Energía cinética rotacional conservada
  Vector3 omega = s.angular_velocity;
  Vector3 I_omega = s.inertia * omega;
  double final_energy = 0.5 * omega.dot(I_omega);
  EXPECT_NEAR(final_energy, initial_energy, 1e-4);
}

// ---------------------------------------------------------------------------
// Test: Precesión libre de cuerpo rígido asimétrico (conservación de energía)
// ---------------------------------------------------------------------------
// Cuerpo con inercia Ixx=10, Iyy=20, Izz=30 y omega_0=(1, 2, 3) rad/s.
// Sin torque externo, la energía cinética rotacional T y el momento angular
// L² = (I*omega)·(I*omega) deben conservarse.
TEST_F(PhysicsIntegratorTest, TorqueFreeAsymmetricBodyEnergyConservation) {
  PhysicsState s;
  s.mass = 100.0;
  s.inertia = makeInertiaTensor(10.0, 20.0, 30.0, 0.0, 0.0, 0.0);
  s.angular_velocity = Vector3(1.0, 2.0, 3.0);

  Vector3 force = Vector3::Zero();
  Vector3 torque = Vector3::Zero();
  double dt = 0.0005;

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

  // Calcular invariantes iniciales
  Vector3 L0 = s.inertia * s.angular_velocity;
  double T0  = 0.5 * s.angular_velocity.dot(L0);
  double L0_sq = L0.squaredNorm();

  // Integrar 5 s (10000 pasos)
  for (int i = 0; i < 10000; ++i) {
    s = integrator->integrate(s, force, torque, dt);
  }

  // Calcular invariantes finales
  Vector3 Lf = s.inertia * s.angular_velocity;
  double Tf  = 0.5 * s.angular_velocity.dot(Lf);
  double Lf_sq = Lf.squaredNorm();

  // Energía cinética rotacional conservada (tolerancia ~0.1%)
  EXPECT_NEAR(Tf, T0, T0 * 1e-3);

  // Magnitud del momento angular conservada (tolerancia ~0.1%)
  EXPECT_NEAR(Lf_sq, L0_sq, L0_sq * 1e-3);

  // Cuaternión permanece unitario
  EXPECT_NEAR(s.orientation.norm(), 1.0, 1e-10);
}

// ---------------------------------------------------------------------------
// Test: Respuesta a torque constante en un solo eje
// ---------------------------------------------------------------------------
// Inercia diagonal Izz = 8 kg·m², torque tau_z = 16 N·m, sin fuerzas.
// Solución analítica:
//   alpha_z = tau_z / Izz = 2 rad/s²
//   omega_z(t) = alpha_z * t = 2 * 2 = 4 rad/s
//   theta_z(t) = 0.5 * alpha_z * t² = 0.5 * 2 * 4 = 4 rad
// (theta no se mide directamente, pero omega sí).
TEST_F(PhysicsIntegratorTest, ConstantTorqueSingleAxis) {
  PhysicsState s;
  s.mass = 100.0;
  s.inertia = makeInertiaTensor(10.0, 12.0, 8.0, 0.0, 0.0, 0.0);

  Vector3 force = Vector3::Zero();
  Vector3 torque(0.0, 0.0, 16.0);      // tau_z = 16 N·m
  double dt = 0.001;
  double t_final = 2.0;
  int steps = static_cast<int>(t_final / dt);

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

  for (int i = 0; i < steps; ++i) {
    s = integrator->integrate(s, force, torque, dt);
  }

  // alpha = 16 / 8 = 2 rad/s²
  // omega_z(2) = 2 * 2 = 4 rad/s
  EXPECT_NEAR(s.angular_velocity.z(), 4.0, 1e-4);
  EXPECT_NEAR(s.angular_velocity.x(), 0.0, 1e-6);
  EXPECT_NEAR(s.angular_velocity.y(), 0.0, 1e-6);
}

// ---------------------------------------------------------------------------
// Test: Fuerza y torque simultáneos (6-DOF combinado)
// ---------------------------------------------------------------------------
// Verifica que la traslación y la rotación evolucionan de forma independiente
// cuando no hay acoplamiento (sin efecto giroscópico significativo).
// F = (0, 0, -mg), tau = (0, tau_y, 0)
TEST_F(PhysicsIntegratorTest, CombinedForceAndTorque6DOF) {
  PhysicsState s;
  s.position = Vector3(0, 0, 1000.0);
  s.velocity = Vector3(50.0, 0.0, 0.0);
  s.mass = 1000.0;
  s.inertia = makeInertiaTensor(10.0, 12.0, 8.0, 0.0, 0.0, 0.0);

  double g = 9.81;
  Vector3 weight(0.0, 0.0, -g * s.mass);
  Vector3 torque(0.0, 24.0, 0.0);     // tau_y = 24 N·m => alpha_y = 24/12 = 2 rad/s²
  double dt = 0.001;
  double t_final = 3.0;
  int steps = static_cast<int>(t_final / dt);

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

  for (int i = 0; i < steps; ++i) {
    s = integrator->integrate(s, weight, torque, dt);
  }

  // Traslación analítica:
  //   x(3) = 50*3 = 150 m
  //   z(3) = 1000 + 0 - 0.5*9.81*9 = 955.855 m
  //   vz(3) = -9.81*3 = -29.43 m/s
  EXPECT_NEAR(s.position.x(), 150.0,       1e-3);
  EXPECT_NEAR(s.position.z(), 1000.0 - 0.5 * g * 9.0, 1e-3);
  EXPECT_NEAR(s.velocity.x(), 50.0,        1e-6);
  EXPECT_NEAR(s.velocity.z(), -g * 3.0,    1e-3);

  // Rotación analítica:
  //   omega_y(3) = 2*3 = 6 rad/s
  EXPECT_NEAR(s.angular_velocity.y(), 6.0, 1e-3);

  // Cuaternión unitario
  EXPECT_NEAR(s.orientation.norm(), 1.0, 1e-10);
}

// ---------------------------------------------------------------------------
// Test: Conservación de cuaternión unitario en integración larga
// ---------------------------------------------------------------------------
TEST_F(PhysicsIntegratorTest, QuaternionRemainsNormalizedAfterTorque) {
  PhysicsState s;
  s.angular_velocity = Vector3(0.1, 0.2, 0.3);
  s.mass = 1.0;

  Vector3 torque(1.0, 2.0, 0.5);
  double dt = 0.01;

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

  for (int i = 0; i < 200; ++i) {
    s = integrator->integrate(s, Vector3::Zero(), torque, dt);
  }

  EXPECT_NEAR(s.orientation.norm(), 1.0, 1e-10);
}

// ---------------------------------------------------------------------------
// Test: Objeto sin fuerza ni torque mantiene movimiento uniforme
// ---------------------------------------------------------------------------
TEST_F(PhysicsIntegratorTest, InertialMotionNoForceNoTorque) {
  PhysicsState s;
  s.position = Vector3(10.0, 20.0, 30.0);
  s.velocity = Vector3(5.0, -3.0, 7.0);
  s.angular_velocity = Vector3(0.0, 0.0, 0.0);
  s.mass = 42.0;

  double dt = 0.01;
  double t_final = 2.0;
  int steps = static_cast<int>(t_final / dt);

  integrator->setIntegratorType(PhysicsIntegrator::IntegratorType::RUNGE_KUTTA_4);

  for (int i = 0; i < steps; ++i) {
    s = integrator->integrate(s, Vector3::Zero(), Vector3::Zero(), dt);
  }

  // Movimiento rectilíneo uniforme: x(t) = x0 + v*t
  EXPECT_NEAR(s.position.x(), 10.0 + 5.0  * t_final, 1e-8);
  EXPECT_NEAR(s.position.y(), 20.0 + (-3.0) * t_final, 1e-8);
  EXPECT_NEAR(s.position.z(), 30.0 + 7.0  * t_final, 1e-8);

  // Velocidad constante
  EXPECT_NEAR(s.velocity.x(),  5.0, 1e-10);
  EXPECT_NEAR(s.velocity.y(), -3.0, 1e-10);
  EXPECT_NEAR(s.velocity.z(),  7.0, 1e-10);
}

// ---------------------------------------------------------------------------
// Test: Conversión FlatBuffer
// ---------------------------------------------------------------------------

TEST_F(PhysicsIntegratorTest, FlatBufferFromGeneralStateConversion) {
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

  EXPECT_DOUBLE_EQ(physics_state.position.x(), 1.0);
  EXPECT_DOUBLE_EQ(physics_state.position.y(), 2.0);
  EXPECT_DOUBLE_EQ(physics_state.position.z(), 3.0);
  EXPECT_DOUBLE_EQ(physics_state.velocity.x(), 4.0);
  EXPECT_DOUBLE_EQ(physics_state.velocity.y(), 5.0);
  EXPECT_DOUBLE_EQ(physics_state.velocity.z(), 6.0);
  EXPECT_DOUBLE_EQ(physics_state.time, 0.5);
}
