#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace propulsion {

using EngineId = std::string;
using TankId = std::string;

/**
 * @brief Error codes returned by the C++ propulsion contract.
 *
 * The boundary adapter maps these values to the integer result expected by
 * plugin_api.h.
 */
enum class ErrorCode : std::int32_t {
    ok = 0,
    invalid_argument = -1,
    not_configured = -2,
    invalid_state = -3,
    invalid_configuration = -4,
    internal_error = -5
};

struct Status {
    ErrorCode code = ErrorCode::ok;
    std::string message;

    explicit operator bool() const noexcept {
        return code == ErrorCode::ok;
    }

    static Status success() {
        return {};
    }
};

struct Vector3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

/**
 * @brief Quaternion representing the body-to-inertial rotation.
 */
struct Quaterniond {
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

enum class EngineState {
    off,
    starting,
    running,
    starved,
    failed
};

struct EnvironmentState {
    double ambient_pressure_pa = 0.0;
    double ambient_temperature_k = 0.0;
    double atmospheric_density_kg_m3 = 0.0;
};

struct VehicleState {
    double simulation_time_s = 0.0;
    double total_mass_kg = 0.0;
    Quaterniond body_to_inertial{};
};

struct EngineCommand {
    double throttle = 0.0;
    Vector3d tvc_angles_rad{};
    bool ignition_requested = false;
    bool shutdown_requested = false;
};

struct TickInput {
    double delta_time_s = 0.0;
    VehicleState vehicle{};
    EnvironmentState environment{};
    std::vector<EngineCommand> engine_commands;
};

struct PropellantRate {
    TankId tank_id;
    double mass_flow_kg_s = 0.0;
};

/**
 * @brief Ideal demand produced by an engine before tank allocation.
 */
struct EngineDemand {
    EngineId engine_id;
    double thrust_n = 0.0;
    double specific_impulse_s = 0.0;
    Vector3d thrust_direction_body{1.0, 0.0, 0.0};
    Vector3d application_point_body_m{};
    std::vector<PropellantRate> requested_propellants;
};

/**
 * @brief Atomic allocation result for one engine during one tick.
 */
struct PropellantAllocation {
    EngineId engine_id;
    double fulfillment_ratio = 0.0;
    std::vector<PropellantRate> delivered_propellants;
};

struct EngineOutput {
    EngineId engine_id;
    EngineState state = EngineState::off;
    Vector3d force_body_n{};
    Vector3d torque_body_nm{};
    double specific_impulse_s = 0.0;
    std::vector<PropellantRate> consumed_propellants;
};

struct TankState {
    TankId tank_id;
    double remaining_mass_kg = 0.0;
    double capacity_kg = 0.0;
};

/**
 * @brief Result of one propulsion step.
 *
 * Translation force is converted to the inertial frame expected by the
 * current integrator. Torque remains in the body frame, where the inertia
 * tensor and angular rates are defined.
 */
struct TickResult {
    Vector3d net_force_inertial_n{};
    Vector3d net_torque_body_nm{};
    std::vector<EngineOutput> engines;
    std::vector<TankState> tanks;
    double consumed_mass_kg = 0.0;
};

struct EngineConfig {
    EngineId id;
    std::string model_type;
    Vector3d mounting_position_body_m{};
    Vector3d nominal_axis_body{1.0, 0.0, 0.0};
    std::unordered_map<std::string, double> numeric_parameters;
    std::unordered_map<std::string, std::string> text_parameters;
};

struct TankConfig {
    TankId id;
    std::string propellant_type;
    double initial_mass_kg = 0.0;
    double capacity_kg = 0.0;
};

struct PropulsionConfig {
    std::vector<EngineConfig> engines;
    std::vector<TankConfig> tanks;
    bool strict_command_count = true;
};

struct PropulsionSnapshot {
    bool configured = false;
    std::vector<EngineState> engine_states;
    std::vector<TankState> tanks;
};

} // namespace propulsion
