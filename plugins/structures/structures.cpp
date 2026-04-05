/*
 * Structures Plugin - Modulo de propiedades inerciales y limites fisicos.
 *
 * Este plugin mantiene una estructura similar a los plugins de aerodinamica
 * y propulsion: crea instancia, configura, procesa tick y destruye.
 *
 * Salida del plugin por tick: solo fuerza y torque.
 */

#include "plugin_api.h"
#include "structures_module.h"
#include "state_vector_generated.h"

#include <algorithm>
#include <atomic>
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

using HostLogFn = void(*)(int32_t, const char*, const char*, void*);

struct StructuresPluginInstance {
    structures::StructuresModule module;
    bool initialized = false;
    bool debug_output = false;
    double previous_speed_m_s = 0.0;
    bool speed_initialized = false;
    bool warned_template_physics = false;
};

std::atomic<HostLogFn> g_host_log_fn{nullptr};
std::atomic<void*> g_host_user_data{nullptr};

/**
 * @brief Emite logs del plugin usando servicios del host cuando están habilitados.
 */
void plugin_log(int32_t level, const char* component, const std::string& message) {
    HostLogFn host_log = g_host_log_fn.load(std::memory_order_acquire);
    if (host_log != nullptr) {
        void* user_data = g_host_user_data.load(std::memory_order_relaxed);
        host_log(level, component, message.c_str(), user_data);
        return;
    }

    // Fallback para pruebas aisladas del plugin sin host.
    std::cout << "[" << (component ? component : "Plugin") << "] " << message << std::endl;
}

} // namespace

extern "C" {

/**
 * @brief Recibe servicios opcionales del host (incluye logger compartido).
 */
PLUGIN_EXPORT void plugin_set_host_services(const PluginHostServices* services) {
    if (services != nullptr && services->log != nullptr) {
        g_host_user_data.store(services->user_data, std::memory_order_relaxed);
        g_host_log_fn.store(services->log, std::memory_order_release);
        return;
    }

    g_host_log_fn.store(nullptr, std::memory_order_release);
    g_host_user_data.store(nullptr, std::memory_order_relaxed);
}

/**
 * @brief Crea la instancia del plugin Structures y carga placeholders iniciales.
 */
PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    auto* instance = new StructuresPluginInstance();

    const bool mass_loaded = instance->module.loadMassPropertiesFromJson(kDefaultMassPropsPath);
    if (!mass_loaded) {
        plugin_log(PLUGIN_LOG_ERROR, "Structures", "Failed to open JSON file: " + std::string(kDefaultMassPropsPath));
        delete instance;
        return nullptr;
    }

    instance->initialized = true;

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

        if (params.contains("mass_properties_path")) {
            mass_json_path = params["mass_properties_path"].get<std::string>();
        }
        if (params.contains("debug_output")) {
            instance->debug_output = params["debug_output"].get<bool>();
        }

        // Masa/CoM/inercia se mantienen temporalmente en JSON placeholder.
        const bool mass_loaded = instance->module.loadMassPropertiesFromJson(mass_json_path);

        if (!mass_loaded) {
            plugin_log(PLUGIN_LOG_ERROR, "Structures", "Failed to open JSON file: " + mass_json_path);
        }

        if (!mass_loaded) {
            return -4;
        }

        plugin_log(PLUGIN_LOG_INFO, "Structures", "Instance configured.");
        plugin_log(PLUGIN_LOG_INFO, "Structures", "Initial mass (kg): " + std::to_string(instance->module.initialMassKg()));
        plugin_log(PLUGIN_LOG_INFO, "Structures", "G-load warning/max: "
            + std::to_string(instance->module.warningGLoad()) + " / " + std::to_string(instance->module.maxGLoad()));
        plugin_log(PLUGIN_LOG_WARNING, "Structures", "Temporary physics model is active in plugin_tick and must be replaced by the final model.");
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

    // Lógica temporal de plantilla: este cálculo simplificado debe reemplazarse
    // por el modelo final del módulo Structures.
    const bool integrity_ok = instance->module.checkStructuralIntegrity(dynamic_pressure, g_force);
    if (!instance->warned_template_physics) {
        plugin_log(PLUGIN_LOG_WARNING, "Structures", "plugin_tick is using temporary template physics logic.");
        instance->warned_template_physics = true;
    }
    if (!integrity_ok && instance->debug_output) {
        plugin_log(PLUGIN_LOG_WARNING, "Structures", "Structural limit exceeded. q="
            + std::to_string(dynamic_pressure) + " Pa, g=" + std::to_string(g_force));
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
