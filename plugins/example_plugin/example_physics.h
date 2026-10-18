#ifndef EXAMPLE_PLUGIN_PHYSICS_H
#define EXAMPLE_PLUGIN_PHYSICS_H

/**
 * @file example_physics.h
 * @brief Pure force model of the Example Plugin (standard library only).
 *
 * Everything that does not need FlatBuffers or JSON lives here so it can be unit tested
 * without building the whole simulator (see `tests/test_example_plugin_physics.cpp`).
 *
 * Conventions (same as the rest of the simulator):
 *  - SI units: kg, m, s, N, m/s^2.
 *  - Vectors are expressed in the inertial frame used by `GeneralState`.
 *  - `gravity` is the acceleration vector stored in `GeneralState.gravity`
 *    (it points "down", e.g. (0, 0, -9.81)). The core only carries it; a module has to
 *    turn it into a force. This plugin does so when `apply_gravity` is enabled.
 */

#include <cmath>
#include <cstdint>
#include <string>

namespace example_plugin {

/** @brief Minimal 3D vector (double precision keeps the model free of float round-off). */
struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

inline Vec3d operator+(const Vec3d& a, const Vec3d& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3d operator-(const Vec3d& a, const Vec3d& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3d operator*(const Vec3d& a, double s) { return {a.x * s, a.y * s, a.z * s}; }

inline double norm(const Vec3d& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

inline bool isFinite(const Vec3d& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

/**
 * @brief Plugin parameters, mirroring `plugins[].parameters` in the configuration file.
 *
 * Defaults keep the plugin inert (no forces, no state changes) so enabling it never
 * alters a trajectory unless the user opts in to a behavior.
 */
struct Params {
    /** Print every tick at DEBUG level (otherwise only every `log_interval_ticks`). */
    bool debug_output = false;

    /** Emit one INFO telemetry line every N ticks. */
    int64_t log_interval_ticks = 100;

    /** Turn `GeneralState.gravity` into a force (F = m * g * gravity_scale). */
    bool apply_gravity = false;

    /** Multiplier for the gravity force (0 = off, 1 = as stored in the state). */
    double gravity_scale = 1.0;

    /** Drag coefficient Cd (dimensionless). 0 disables drag. */
    double drag_coefficient = 0.0;

    /** Reference area A in m^2 used by the drag model. */
    double reference_area = 0.5;

    /**
     * Sequential mode only: shift the vehicle position along +X by `step_size` metres per
     * tick. Demonstrates a plugin editing the state buffer directly. Ignored (with a
     * warning) in parallel mode because parallel plugins must only read the state.
     */
    bool enable_state_nudge = false;

    /** Metres added to position X per tick when `enable_state_nudge` is on. */
    double step_size = 1.0;
};

/**
 * @brief Checks that parameter values are physically meaningful.
 *
 * @param params Parameters to validate.
 * @param error  Receives a human readable reason when validation fails (may be null).
 * @return true when every value is acceptable.
 */
inline bool validate(const Params& params, std::string* error = nullptr) {
    auto fail = [&](const char* message) {
        if (error) {
            *error = message;
        }
        return false;
    };

    if (params.log_interval_ticks <= 0) {
        return fail("log_interval_ticks must be a positive integer");
    }
    if (!std::isfinite(params.gravity_scale) || params.gravity_scale < 0.0) {
        return fail("gravity_scale must be a finite number >= 0");
    }
    if (!std::isfinite(params.drag_coefficient) || params.drag_coefficient < 0.0) {
        return fail("drag_coefficient must be a finite number >= 0");
    }
    if (!std::isfinite(params.reference_area) || params.reference_area <= 0.0) {
        return fail("reference_area must be a finite number > 0");
    }
    if (!std::isfinite(params.step_size)) {
        return fail("step_size must be a finite number");
    }
    return true;
}

/** @brief Inputs read from `GeneralState` for one tick. */
struct TickInput {
    double mass = 0.0;       ///< total_mass [kg]
    Vec3d gravity;           ///< gravity acceleration vector [m/s^2]
    Vec3d velocity;          ///< inertial velocity [m/s]
    Vec3d wind;              ///< wind velocity [m/s]
    double air_density = 0.0;///< atm_density [kg/m^3]
};

/** @brief Result of the force model for one tick. */
struct Forces {
    Vec3d gravity;  ///< gravity force [N]
    Vec3d drag;     ///< drag force [N]
    Vec3d total;    ///< sum of all forces [N]
};

/**
 * @brief Gravity force on a body: F = m * g * scale.
 */
inline Vec3d gravityForce(double mass, const Vec3d& gravity, double scale) {
    return gravity * (mass * scale);
}

/**
 * @brief Quadratic aerodynamic drag: F = -1/2 * rho * Cd * A * |v_rel| * v_rel.
 *
 * `v_rel` is the velocity relative to the air mass (vehicle velocity minus wind), so a
 * tailwind that matches the vehicle speed produces no drag. The force always opposes
 * `v_rel`.
 */
inline Vec3d dragForce(double air_density, double drag_coefficient, double reference_area,
                       const Vec3d& relative_velocity) {
    const double speed = norm(relative_velocity);
    const double k = -0.5 * air_density * drag_coefficient * reference_area * speed;
    return relative_velocity * k;
}

/**
 * @brief Computes every force this plugin contributes during one tick.
 *
 * @return true on success. false when the inputs are not usable (non finite values, or a
 *         non positive mass while gravity is requested); `forces` is zeroed in that case.
 */
inline bool computeForces(const Params& params, const TickInput& in, Forces& forces,
                          std::string* error = nullptr) {
    forces = Forces{};

    const bool finite = std::isfinite(in.mass) && std::isfinite(in.air_density) &&
                        isFinite(in.gravity) && isFinite(in.velocity) && isFinite(in.wind);
    if (!finite) {
        if (error) {
            *error = "state contains non finite values";
        }
        return false;
    }

    if (params.apply_gravity) {
        if (in.mass <= 0.0) {
            if (error) {
                *error = "total_mass must be positive to apply gravity";
            }
            return false;
        }
        forces.gravity = gravityForce(in.mass, in.gravity, params.gravity_scale);
    }

    if (params.drag_coefficient > 0.0) {
        forces.drag = dragForce(in.air_density, params.drag_coefficient, params.reference_area,
                                in.velocity - in.wind);
    }

    forces.total = forces.gravity + forces.drag;
    return true;
}

} // namespace example_plugin

#endif // EXAMPLE_PLUGIN_PHYSICS_H
