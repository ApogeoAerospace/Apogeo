#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <boost/numeric/odeint.hpp>
#include "flatbuffers/flatbuffers.h"
#include "state_vector_generated.h"

namespace MoLab {

// Tipo de estado plano para odeint: [pos(3), vel(3), quat(4), omega(3)] = 13 elementos.
using OdeState = std::array<double, 13>;

// Alias de Eigen para tipos matemáticos internos.
// Se usan por sus operaciones de álgebra lineal (producto cuaternión,
// producto cruz, descomposición LDLT del tensor de inercia), no solo
// como contenedores de datos.
using Vector3 = Eigen::Vector3d;
using Quaternion4 = Eigen::Quaterniond;
using InertiaTensor3 = Eigen::Matrix3d;

// ---------------------------------------------------------------------------
// Funciones auxiliares de conversión entre OdeState y tipos Eigen
// ---------------------------------------------------------------------------

// Disposición del vector plano:
//   [0..2]  = posición  (x, y, z)
//   [3..5]  = velocidad (vx, vy, vz)
//   [6..9]  = orientación cuaternión (w, x, y, z)
//   [10..12] = velocidad angular (wx, wy, wz)

inline Vector3 odePosition(const OdeState& y) {
    return Vector3(y[0], y[1], y[2]);
}

inline Vector3 odeVelocity(const OdeState& y) {
    return Vector3(y[3], y[4], y[5]);
}

inline Quaternion4 odeOrientation(const OdeState& y) {
    return Quaternion4(y[6], y[7], y[8], y[9]);
}

inline Vector3 odeAngularVelocity(const OdeState& y) {
    return Vector3(y[10], y[11], y[12]);
}

inline void odePackVec3(OdeState& y, int offset, const Vector3& v) {
    y[offset]     = v.x();
    y[offset + 1] = v.y();
    y[offset + 2] = v.z();
}

inline void odePackQuat(OdeState& y, const Quaternion4& q) {
    y[6] = q.w(); y[7] = q.x(); y[8] = q.y(); y[9] = q.z();
}

// Construir tensor de inercia simétrico 3x3 a partir de los 6 componentes independientes.
inline InertiaTensor3 makeInertiaTensor(double ixx, double iyy, double izz,
                                        double ixy, double ixz, double iyz) {
    InertiaTensor3 I;
    I << ixx, ixy, ixz,
         ixy, iyy, iyz,
         ixz, iyz, izz;
    return I;
}

// Verifica si el tensor de inercia es diagonal (productos de inercia ~0).
inline bool isInertiaDiagonal(const InertiaTensor3& I, double tol = 1e-10) {
    return std::abs(I(0, 1)) < tol && std::abs(I(0, 2)) < tol && std::abs(I(1, 2)) < tol;
}

// Estado físico para integración numérica.
struct PhysicsState {
    Vector3 position;
    Vector3 velocity;
    Quaternion4 orientation;         // Cuaternión unitario (w, x, y, z)
    Vector3 angular_velocity;
    double mass;
    InertiaTensor3 inertia;          // Tensor de inercia completo
    double time;

    PhysicsState()
        : position(Vector3::Zero()),
          velocity(Vector3::Zero()),
          orientation(Quaternion4::Identity()),
          angular_velocity(Vector3::Zero()),
          mass(1.0),
          inertia(InertiaTensor3::Identity()),
          time(0.0) {}

    // Empaquetar variables dinámicas en vector plano para odeint.
    OdeState toOdeState() const;

    // Desempaquetar vector plano de odeint a las variables dinámicas.
    void fromOdeState(const OdeState& y);
};

// Integrador físico delegando a Boost.Odeint (Euler/RK4/Velocity-Verlet).
class PhysicsIntegrator {
public:
    enum class IntegratorType {
        EULER,
        RUNGE_KUTTA_4,
        VERLET
    };

    PhysicsIntegrator(IntegratorType type = IntegratorType::RUNGE_KUTTA_4);
    ~PhysicsIntegrator() = default;

    // Función principal de integración
    PhysicsState integrate(const PhysicsState& current_state,
                          const Vector3& total_force,
                          const Vector3& total_torque,
                          double dt);

    // Configurar tipo de integrador
    void setIntegratorType(IntegratorType type) { integrator_type_ = type; }
    void setIntegratorType(const std::string& type_name);

    // Obtener tipo de integrador actual
    IntegratorType getIntegratorType() const { return integrator_type_; }
    std::string getIntegratorTypeName() const;

    // Conversión desde FlatBuffer
    static PhysicsState fromFlatBuffer(const state_vector::GeneralState* fb_state);

private:
    IntegratorType integrator_type_;

    // Calcula las derivadas del OdeState directamente, evitando reconstrucción
    // completa de PhysicsState en cada evaluación del sistema ODE.
    // Los parámetros constantes (masa, inercia, fuerza, torque) se pasan
    // por referencia para evitar copias innecesarias.
    static void computeOdeDerivatives(const OdeState& y, OdeState& dydt,
                                      double mass,
                                      const InertiaTensor3& inertia,
                                      const Vector3& force,
                                      const Vector3& torque);
};

} // namespace MoLab
