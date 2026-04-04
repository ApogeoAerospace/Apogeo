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

/**
 * @file PhysicsIntegrator.h
 * @brief Integración numérica del estado físico de simulación.
 */

namespace MoLab {

/**
 * @brief Tipo de estado plano para ODE: `pos(3), vel(3), quat(4), omega(3)`.
 */
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

/**
 * @struct PhysicsState
 * @brief Estado físico interno utilizado por el integrador numérico.
 */
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

    /**
     * @brief Empaqueta el estado en formato plano para `odeint`.
     * @return Estado plano equivalente.
     */
    OdeState toOdeState() const;

    /**
     * @brief Carga variables dinámicas desde un estado plano.
     * @param y Estado plano fuente.
     */
    void fromOdeState(const OdeState& y);
};

/**
 * @class PhysicsIntegrator
 * @brief Integrador físico basado en Boost.Odeint.
 */
class PhysicsIntegrator {
public:
    /**
     * @enum IntegratorType
     * @brief Algoritmos de integración disponibles.
     */
    enum class IntegratorType {
        EULER,
        RUNGE_KUTTA_4,
        VERLET
    };

    /**
     * @brief Construye un integrador con un método inicial.
     * @param type Tipo de integrador a usar.
     */
    PhysicsIntegrator(IntegratorType type = IntegratorType::RUNGE_KUTTA_4);
    ~PhysicsIntegrator() = default;

    /**
     * @brief Integra el estado físico durante un paso temporal.
     * @param current_state Estado actual.
     * @param total_force Fuerza total aplicada.
     * @param total_torque Torque total aplicado.
     * @param dt Paso de integración en segundos.
     * @return Nuevo estado integrado.
     */
    PhysicsState integrate(const PhysicsState& current_state,
                          const Vector3& total_force,
                          const Vector3& total_torque,
                          double dt);

    /**
     * @brief Cambia el integrador activo por enumeración.
     * @param type Tipo de integrador.
     */
    void setIntegratorType(IntegratorType type) { integrator_type_ = type; }

    /**
     * @brief Cambia el integrador activo por nombre.
     * @param type_name Nombre del método (`euler`, `runge_kutta_4`, `verlet`).
     */
    void setIntegratorType(const std::string& type_name);

    /**
     * @brief Obtiene el tipo de integrador activo.
     * @return Tipo de integrador actual.
     */
    IntegratorType getIntegratorType() const { return integrator_type_; }

    /**
     * @brief Obtiene el nombre del integrador activo.
     * @return Nombre del método de integración.
     */
    std::string getIntegratorTypeName() const;

    /**
     * @brief Convierte un estado FlatBuffers a estado físico interno.
     * @param fb_state Estado serializado.
     * @return Estado físico para integración.
     */
    static PhysicsState fromFlatBuffer(const state_vector::GeneralState* fb_state);

private:
    IntegratorType integrator_type_;

    /**
     * @brief Calcula derivadas del sistema ODE para integración.
     */
    static void computeOdeDerivatives(const OdeState& y, OdeState& dydt,
                                      double mass,
                                      const InertiaTensor3& inertia,
                                      const Vector3& force,
                                      const Vector3& torque);
};

} // namespace MoLab
