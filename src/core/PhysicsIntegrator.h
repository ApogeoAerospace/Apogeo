#pragma once

#include <cmath>
#include <vector>
#include <memory>
#include <functional>
#include "flatbuffers/flatbuffers.h"
#include "state_vector_generated.h"

namespace MoLab {

struct Vector3 {
    double x, y, z;

    Vector3() : x(0), y(0), z(0) {}
    Vector3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }

    Vector3 operator*(double scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    Vector3& operator+=(const Vector3& other) {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }

    double magnitude() const {
        return sqrt(x*x + y*y + z*z);
    }

    Vector3 normalized() const {
        double mag = magnitude();
        if (mag > 0) return Vector3(x/mag, y/mag, z/mag);
        return Vector3();
    }
};

struct StateDerivative {
    Vector3 velocity;        // d(position)/dt
    Vector3 acceleration;    // d(velocity)/dt
    Vector3 angular_velocity; // d(orientation)/dt (simplified)
    Vector3 angular_acceleration; // d(angular_velocity)/dt
};

struct PhysicsState {
    Vector3 position;
    Vector3 velocity;
    Vector3 orientation; // Euler angles (simplified)
    Vector3 angular_velocity;
    double mass;
    double time;

    PhysicsState() : mass(1.0), time(0.0) {}
};

class PhysicsIntegrator {
public:
    enum class IntegratorType {
        EULER,
        RUNGE_KUTTA_4,
        VERLET
    };

    PhysicsIntegrator(IntegratorType type = IntegratorType::RUNGE_KUTTA_4);
    ~PhysicsIntegrator() = default;

    // Main integration function
    PhysicsState integrate(const PhysicsState& current_state,
                          const Vector3& total_force,
                          const Vector3& total_torque,
                          double dt);

    // Set integrator type
    void setIntegratorType(IntegratorType type) { integrator_type_ = type; }
    void setIntegratorType(const std::string& type_name);

    // Get current integrator type
    IntegratorType getIntegratorType() const { return integrator_type_; }
    std::string getIntegratorTypeName() const;

    // Utility functions for state conversion
    static PhysicsState fromFlatBuffer(const state_vector::GeneralState* fb_state);
    static void toFlatBuffer(flatbuffers::FlatBufferBuilder& builder,
                            const PhysicsState& state);

private:
    IntegratorType integrator_type_;

    // Integration methods
    PhysicsState integrateEuler(const PhysicsState& state,
                               const Vector3& force,
                               const Vector3& torque,
                               double dt);

    PhysicsState integrateRungeKutta4(const PhysicsState& state,
                                     const Vector3& force,
                                     const Vector3& torque,
                                     double dt);

    PhysicsState integrateVerlet(const PhysicsState& state,
                                const Vector3& force,
                                const Vector3& torque,
                                double dt);

    // Derivative calculation
    StateDerivative calculateDerivative(const PhysicsState& state,
                                       const Vector3& force,
                                       const Vector3& torque);

    // Previous state for Verlet integration
    PhysicsState previous_state_;
    bool has_previous_state_;
};

// Utility functions for atmospheric effects
namespace AtmosphericEffects {
    Vector3 calculateDrag(const Vector3& velocity, double air_density,
                         double drag_coefficient, double reference_area);

    Vector3 calculateWind(const Vector3& wind_velocity, const Vector3& object_velocity,
                         double air_density, double reference_area);

    double calculateAirDensity(double altitude, double temperature = 288.15);
}

// Utility functions for gravitational effects
namespace GravitationalEffects {
    Vector3 calculateEarthGravity(const Vector3& position, double mass = 1.0);
    Vector3 calculateCentralGravity(const Vector3& position, double central_mass,
                                   double gravitational_constant = 6.67430e-11);
}

} // namespace MoLab
