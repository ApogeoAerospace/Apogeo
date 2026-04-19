#ifndef STATE_VECTOR_DOCS_H
#ifndef STATE_VECTOR_DOCS_H
#define STATE_VECTOR_DOCS_H

/**
 * @file state_vector_docs.h
 * @brief Doxygen documentation for the FlatBuffers-serialized state model.
 */

/**
 * @namespace state_vector
 * @brief FlatBuffers types representing global simulation state.
 *
 * This namespace is generated from schema `src/schemas/state_vector.fbs`.
 * It contains basic mathematical structures and root table `GeneralState`.
 */
namespace state_vector {

/**
 * @struct Vec3
 * @brief Vector 3D (x, y, z).
 */
struct Vec3;

/**
 * @struct Quaternion
 * @brief Orientation quaternion (x, y, z, w).
 */
struct Quaternion;

/**
 * @struct InertiaTensor
 * @brief Vehicle inertia tensor in its local reference frame.
 */
struct InertiaTensor;

/**
 * @struct EngineCmd
 * @brief Per-engine command: throttle level and TVC angles.
 */
struct EngineCmd;

/**
 * @struct GeneralState
 * @brief Root table with complete simulation state.
 *
 * Includes kinematics, dynamics, aerodynamic properties,
 * atmospheric environment, and actuator commands.
 *
 * Fields by section:
 * - Time and integration: `sim_time`, `dt`
 * - Kinematics: `position`, `velocity`, `orientation`, `angular_velocity`
 * - Dynamics: `total_mass`, `cg_location`, `inertia_tensor`, `propellant_masses`
 * - Aerodynamics: `mach_number`, `dynamic_pressure`, `angle_of_attack`, `sideslip_angle`
 * - Environment: `atm_density`, `atm_pressure`, `atm_temperature`, `wind_velocity`, `gravity`
 * - Control/actuation: `engines`, `surface_deflections`
 */
struct GeneralState;

} // namespace state_vector

#endif // STATE_VECTOR_DOCS_H
