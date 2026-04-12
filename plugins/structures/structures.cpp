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

    const bool mass_loaded = instance->module.loadMassPropertiesFromJson(kDefaultMassPropsPath);
    const bool limits_loaded = instance->module.loadStructuralLimitsFromCsv(kDefaultStructuralLimitsPath);
    if (!mass_loaded || !limits_loaded) {
        std::cout << "[Structures] Error: no se pudo inicializar con datos por defecto."
                  << " mass_loaded=" << mass_loaded
                  << " limits_loaded=" << limits_loaded << std::endl;
        delete instance;
        return nullptr;
    }
    instance->initialized = true;

    std::cout << "[Structures] Instancia creada." << std::endl;
    std::cout << "[Structures] Masa inicial (kg): " << instance->module.initialMassKg() << std::endl;
    std::cout << "[Structures] Limite G warning/max: "
              << instance->module.warningGLoad() << " / "
              << instance->module.maxGLoad() << std::endl;

    return reinterpret_cast<PluginHandle>(instance);
}

/**
 * @brief Configura rutas de datos estructurales desde configuracion general. (No actuadores)
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
        const bool mass_loaded = instance->module.loadMassPropertiesFromJson(mass_json_path);
        const bool limits_loaded = instance->module.loadStructuralLimitsFromCsv(limits_csv_path);
        if (!mass_loaded || !limits_loaded) {
            if (instance->debug_output) {
                std::cout << "[Structures] Error: configuracion invalida de fuentes estructurales."
                          << " mass_loaded=" << mass_loaded
                          << " limits_loaded=" << limits_loaded << std::endl;
            }
            return -4;
        }
        return 0;
    } catch (...) {
        return -3;
    }
}

/**
 * @brief Ejecuta un tick estructural tipo 1 (solo lectura de estado, salida fuerza/torque).
 *
 * @warning Implementacion donde no se aplica logica fisica activa.
 * Devuelve salida neutra (cero) para preservar compatibilidad del flujo.
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

    // No hay salida fisica activa por ahora, estilo template
    force_output->x = 0.0f;
    force_output->y = 0.0f;
    force_output->z = 0.0f;

    if (torque_output) {
        torque_output->x = 0.0f;
        torque_output->y = 0.0f;
        torque_output->z = 0.0f;
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
