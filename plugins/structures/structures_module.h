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
 * @brief Dominio del modulo Structures: datos de masa, limites y calculo estructural.
 */
class StructuresModule {
public:
    /**
     * @brief Carga propiedades de masa placeholder para el modulo Structures.
     *
     * @details
     * La fuente para masa actual, centro de masa e inercia es el
     * buffer principal de simulacion. Mientras esa integracion no exista,
     * se permite esta carga desde JSON como placeholder temporal.
     *
     * Nota: los actuadores NO se toman desde este JSON. La fuente oficial de
     * actuadores es la configuracion general recibida en plugin_configure().
     */
    bool loadMassPropertiesFromJson(const std::string& json_path);

    /**
     * @brief Carga limites estructurales desde CSV.
     */
    bool loadStructuralLimitsFromCsv(const std::string& csv_path);

    Vec3d getCenterOfMass() const;
    InertiaTensorData getInertiaTensor() const;

    /**
     * @brief Verifica limites maximos de integridad estructural.
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
