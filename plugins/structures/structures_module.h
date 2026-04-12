#pragma once

#include "plugin_api.h"
#include "state_vector_generated.h"

#include <string>

namespace structures {

struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct InertiaTensorData {
    double ixx = 0.0;
    double iyy = 0.0;
    double izz = 0.0;
    double ixy = 0.0;
    double ixz = 0.0;
    double iyz = 0.0;
};

struct StructuralLimits {
    double max_g_load = 4.0;
    double warning_g_load = 3.5;
    double max_dynamic_pressure = 50000.0;
    double warning_dynamic_pressure = 40000.0;
};

/**
 * @brief Structures module domain: mass data, limits, and structural calculations.
 */
class StructuresModule {
public:
    /**
     * @brief Loads mass properties from JSON.
     *
     * @details
     * Supports initial mass/fuel mass, center of mass, and inertia tensor
     * keys used by the current Structures module implementation.
     */
    bool loadMassPropertiesFromJson(const std::string& json_path);

    /**
     * @brief Loads structural limits from CSV key/value rows.
     */
    bool loadStructuralLimitsFromCsv(const std::string& csv_path);

    Vec3d getCenterOfMass() const;
    InertiaTensorData getInertiaTensor() const;

    /**
     * @brief Checks maximum structural integrity limits.
     */
    bool checkStructuralIntegrity(double dynamic_pressure, double g_force) const;

    PluginVector3 computeStructuralForce(const state_vector::GeneralState* state) const;
    PluginVector3 computeStructuralTorque(const state_vector::GeneralState* state) const;

    double initialMassKg() const;
    double maxGLoad() const;
    double warningGLoad() const;

private:
    double initial_total_mass_kg_ = 1000.0;
    double fuel_mass_kg_ = 0.0;
    Vec3d center_of_mass_{};
    InertiaTensorData inertia_tensor_{};
    StructuralLimits limits_{};
};

} // namespace structures
