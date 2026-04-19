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
 * @brief Numerical integration of simulation physical state.
 */

namespace MoLab {

/**
 * @brief Flat ODE state type: `pos(3), vel(3), quat(4), omega(3)`.
 */
using OdeState = std::array<double, 13>;

// Eigen aliases for internal mathematical types.
// Used for linear algebra operations (quaternion product,
// cross product, LDLT decomposition of inertia tensor), not only
// as data containers.
using Vector3 = Eigen::Vector3d;
using Quaternion4 = Eigen::Quaterniond;
using InertiaTensor3 = Eigen::Matrix3d;

// ---------------------------------------------------------------------------
// Helper conversion functions between OdeState and Eigen types
// ---------------------------------------------------------------------------

// Flat vector layout:
//   [0..2]  = position  (x, y, z)
//   [3..5]  = velocity (vx, vy, vz)
//   [6..9]  = quaternion orientation (w, x, y, z)
//   [10..12] = angular velocity (wx, wy, wz)

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

// Build symmetric 3x3 inertia tensor from 6 independent components.
inline InertiaTensor3 makeInertiaTensor(double ixx, double iyy, double izz,
                                        double ixy, double ixz, double iyz) {
    InertiaTensor3 I;
    I << ixx, ixy, ixz,
         ixy, iyy, iyz,
         ixz, iyz, izz;
    return I;
}

    // Checks whether inertia tensor is diagonal (inertia products ~0).
inline bool isInertiaDiagonal(const InertiaTensor3& I, double tol = 1e-10) {
    return std::abs(I(0, 1)) < tol && std::abs(I(0, 2)) < tol && std::abs(I(1, 2)) < tol;
}

/**
 * @struct PhysicsState
 * @brief Internal physical state used by the numerical integrator.
 */
struct PhysicsState {
    Vector3 position;
    Vector3 velocity;
    Vector3 gravity;
    Quaternion4 orientation;         // Unit quaternion (w, x, y, z)
    Vector3 angular_velocity;
    double mass;
    InertiaTensor3 inertia;          // Full inertia tensor
    double time;

    PhysicsState()
        : position(Vector3::Zero()),
          velocity(Vector3::Zero()),
          gravity(Vector3::Zero()),
          orientation(Quaternion4::Identity()),
          angular_velocity(Vector3::Zero()),
          mass(1.0),
          inertia(InertiaTensor3::Identity()),
          time(0.0) {}

    /**
     * @brief Packs state into flat format for `odeint`.
     * @return Equivalent flat state.
     */
    OdeState toOdeState() const;

    /**
     * @brief Loads dynamic variables from a flat state.
     * @param y Source flat state.
     */
    void fromOdeState(const OdeState& y);
};

/**
 * @class PhysicsIntegrator
 * @brief Physics integrator based on Boost.Odeint.
 */
class PhysicsIntegrator {
public:
    /**
     * @enum IntegratorType
     * @brief Available integration algorithms.
     */
    enum class IntegratorType {
        EULER,
        RUNGE_KUTTA_4,
        VERLET
    };

    /**
     * @brief Constructs integrator with an initial method.
     * @param type Integrator type to use.
     */
    PhysicsIntegrator(IntegratorType type = IntegratorType::RUNGE_KUTTA_4);
    ~PhysicsIntegrator() = default;

    /**
     * @brief Integrates physical state over one time step.
     * @param current_state Current state.
     * @param total_force Total applied force.
     * @param total_torque Total applied torque.
     * @param dt Integration step in seconds.
     * @return New integrated state.
     */
    PhysicsState integrate(const PhysicsState& current_state,
                          const Vector3& total_force,
                          const Vector3& total_torque,
                          double dt);

    /**
     * @brief Changes active integrator by enum.
     * @param type Integrator type.
     */
    void setIntegratorType(IntegratorType type) { integrator_type_ = type; }

    /**
     * @brief Changes active integrator by name.
     * @param type_name Method name (`euler`, `runge_kutta_4`, `verlet`).
     */
    void setIntegratorType(const std::string& type_name);

    /**
     * @brief Gets active integrator type.
     * @return Current integrator type.
     */
    IntegratorType getIntegratorType() const { return integrator_type_; }

    /**
     * @brief Gets active integrator name.
     * @return Integration method name.
     */
    std::string getIntegratorTypeName() const;

    /**
     * @brief Converts FlatBuffers state into internal physical state.
     * @param fb_state Serialized state.
     * @return Physical state for integration.
     */
    static PhysicsState fromFlatBuffer(const state_vector::GeneralState* fb_state);

private:
    IntegratorType integrator_type_;

    /**
     * @brief Computes ODE system derivatives for integration.
     */
    static void computeOdeDerivatives(const OdeState& y, OdeState& dydt,
                                      double mass,
                                      const InertiaTensor3& inertia,
                                      const Vector3& gravity,
                                      const Vector3& force,
                                      const Vector3& torque);
};

} // namespace MoLab
