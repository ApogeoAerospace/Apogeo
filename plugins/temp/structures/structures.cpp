/*
 * Structures Plugin - Modulo de propiedades inerciales y limites fisicos.
 *
 * Este plugin mantiene una estructura similar a los plugins de aerodinamica
 * y propulsion: crea instancia, configura, procesa tick y destruye.
 *
 * Salida del plugin por tick: solo fuerza y torque.
 */

#include "../../src/api/plugin_api.h"
#include "structures_module.h"
#include "state_vector_generated.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

namespace {

constexpr double kStandardGravity = 9.80665;
constexpr double kEarthRadiusM = 6371000.0;
constexpr double kSeaLevelDensity = 1.225;
constexpr double kScaleHeight = 8400.0;

constexpr const char* kDefaultMassPropsPath = "data/mass/mass_properties.json";
constexpr const char* kDefaultStructuralLimitsPath = "data/limits/structural_limits.csv";

struct StructuresPluginInstance {
    structures::StructuresModule module;
    bool initialized = false;
    bool debug_output = false;
    double previous_speed_m_s = 0.0;
    bool speed_initialized = false;
};

} // namespace

extern "C" {

/**
 * @brief Crea la instancia del plugin Structures y carga placeholders iniciales.
 */
PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    auto* instance = new StructuresPluginInstance();

    instance->module.loadMassPropertiesFromJson(kDefaultMassPropsPath);
    instance->module.loadStructuralLimitsFromCsv(kDefaultStructuralLimitsPath);
    instance->module.mapActuatorsPlaceholder();
    instance->initialized = true;

    std::cout << "[Structures] Instancia creada." << std::endl;
    std::cout << "[Structures] Masa inicial (kg): " << instance->module.initialMassKg() << std::endl;
    std::cout << "[Structures] Limite G warning/max: "
              << instance->module.warningGLoad() << " / "
              << instance->module.maxGLoad() << std::endl;

    return reinterpret_cast<PluginHandle>(instance);
}

/**
 * @brief Configura rutas de datos y actuadores desde configuracion general.
 */
PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params) {
    if (!handle || !json_params) {
        return -1;
    }

    auto* instance = reinterpret_cast<StructuresPluginInstance*>(handle);
    if (!instance->initialized) {
        return -2;
    }

    try {
        const nlohmann::json params = nlohmann::json::parse(json_params);

        std::string mass_json_path = kDefaultMassPropsPath;
        std::string limits_csv_path = kDefaultStructuralLimitsPath;

        if (params.contains("mass_properties_path")) {
            mass_json_path = params["mass_properties_path"].get<std::string>();
        }
        if (params.contains("structural_limits_path")) {
            limits_csv_path = params["structural_limits_path"].get<std::string>();
        }
        if (params.contains("debug_output")) {
            instance->debug_output = params["debug_output"].get<bool>();
        }

        // Masa/CoM/inercia se mantienen temporalmente en JSON placeholder.
        // Actuadores se leen solo desde configuracion general (params["actuators"]).
        instance->module.loadMassPropertiesFromJson(mass_json_path);
        instance->module.loadStructuralLimitsFromCsv(limits_csv_path);
        if (params.contains("actuators")) {
            instance->module.setActuatorsFromJsonArray(params["actuators"]);
        }
        instance->module.mapActuatorsPlaceholder();
        return 0;
    } catch (...) {
        return -3;
    }
}

/**
 * @brief Ejecuta un tick estructural tipo 1 (solo lectura de estado, salida fuerza/torque).
 */
PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data || !data->state_buffer) {
        return -1;
    }

    PluginVector3* force_output = data->output_force ? data->output_force : data->force_out;
    PluginVector3* torque_output = data->output_torque ? data->output_torque : data->torque_out;
    if (!force_output) {
        return -1;
    }

    auto* instance = reinterpret_cast<StructuresPluginInstance*>(handle);
    if (!instance->initialized) {
        return -2;
    }

    const auto* state = state_vector::GetGeneralState(data->state_buffer);
    if (!state || !state->position() || !state->velocity()) {
        return -3;
    }

    const double vx = state->velocity()->x();
    const double vy = state->velocity()->y();
    const double vz = state->velocity()->z();
    const double speed = std::sqrt(vx * vx + vy * vy + vz * vz);

    // Primer tick: warm-up de velocidad previa para evitar pico artificial de g.
    if (!instance->speed_initialized) {
        instance->previous_speed_m_s = speed;
        instance->speed_initialized = true;

        *force_output = instance->module.computeStructuralForce(state);
        if (torque_output) {
            *torque_output = instance->module.computeStructuralTorque(state);
        }
        return 0;
    }

    const double ax = data->delta_time > 0.0 ? (speed - instance->previous_speed_m_s) / data->delta_time : 0.0;
    instance->previous_speed_m_s = speed;
    const double g_force = std::abs(ax) / kStandardGravity;

    const double px = state->position()->x();
    const double py = state->position()->y();
    const double pz = state->position()->z();
    double altitude = std::sqrt(px * px + py * py + pz * pz) - kEarthRadiusM;
    altitude = std::max(0.0, altitude);

    const double air_density = kSeaLevelDensity * std::exp(-altitude / kScaleHeight);
    const double dynamic_pressure = 0.5 * air_density * speed * speed;

    // Tick >= 2: evaluacion de integridad con g y presion dinamica actuales.
    const bool integrity_ok = instance->module.checkStructuralIntegrity(dynamic_pressure, g_force);
    if (!integrity_ok && instance->debug_output) {
        std::cout << "[Structures] Warning: limite estructural excedido. q="
                  << dynamic_pressure << " Pa, g=" << g_force << std::endl;
    }

    *force_output = instance->module.computeStructuralForce(state);

    if (torque_output) {
        *torque_output = instance->module.computeStructuralTorque(state);
    }

    return 0;
}

/**
 * @brief Libera la instancia del plugin Structures.
 */
PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (!handle) {
        return;
    }
    auto* instance = reinterpret_cast<StructuresPluginInstance*>(handle);
    delete instance;
}

} // extern "C"
