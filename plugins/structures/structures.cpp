/*
 * Structures Plugin - Inertial properties and physical limits module.
 *
 * This plugin keeps a similar structure to the aerodynamics and propulsion plugins:
 * create instance, configure, process tick, and destroy.
 *
 * Plugin output per tick: force and torque only.
 */

#include "plugin_api.h"
#include "structures_module.h"
#include "state_vector_generated.h"

#include <atomic>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

namespace {

constexpr const char* kDefaultMassPropsPath = "data/mass/mass_properties.json";
constexpr const char* kDefaultStructuralLimitsPath = "data/limits/structural_limits.csv";
constexpr uint32_t kSupportedHostServicesApiVersion = 1;

using HostLogFn = void(*)(int32_t, const char*, const char*, void*);

struct StructuresPluginInstance {
    structures::StructuresModule module;
    bool initialized = false;
    bool debug_output = false;
};

std::atomic<HostLogFn> g_host_log_fn{nullptr};
std::atomic<void*> g_host_user_data{nullptr};
std::atomic<bool> g_host_api_mismatch_warned{false};

/**
 * @brief Emits plugin logs through host services when enabled.
 */
void plugin_log(int32_t level, const char* component, const std::string& message) {
    HostLogFn host_log = g_host_log_fn.load(std::memory_order_acquire);
    if (host_log != nullptr) {
        void* user_data = g_host_user_data.load(std::memory_order_relaxed);
        host_log(level, component, message.c_str(), user_data);
        return;
    }

    // Fallback for isolated plugin tests without host.
    std::cout << "[" << (component ? component : "Plugin") << "] " << message << std::endl;
}

} // namespace

extern "C" {

/**
 * @brief Receives optional host services (includes shared logger).
 */
PLUGIN_EXPORT void plugin_set_host_services(const PluginHostServices* services) {
    if (services != nullptr && services->log != nullptr) {
        // If api_version mismatches, fallback to local plugin logging behavior.
        if (services->api_version != kSupportedHostServicesApiVersion) {
            g_host_log_fn.store(nullptr, std::memory_order_release);
            g_host_user_data.store(nullptr, std::memory_order_relaxed);

            bool expected = false;
            if (g_host_api_mismatch_warned.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                std::cout << "[Structures] WARNING: Host services api_version mismatch (host="
                          << services->api_version
                          << ", plugin-supported="
                          << kSupportedHostServicesApiVersion
                          << "). Falling back to local plugin logging." << std::endl;
            }
            return;
        }

        g_host_api_mismatch_warned.store(false, std::memory_order_release);
        g_host_user_data.store(services->user_data, std::memory_order_relaxed);
        g_host_log_fn.store(services->log, std::memory_order_release);
        return;
    }

    g_host_log_fn.store(nullptr, std::memory_order_release);
    g_host_user_data.store(nullptr, std::memory_order_relaxed);
}

/**
 * @brief Creates Structures plugin instance and loads initial placeholders.
 */
PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    auto* instance = new StructuresPluginInstance();

    const bool mass_loaded = instance->module.loadMassPropertiesFromJson(kDefaultMassPropsPath);
    const bool limits_loaded = instance->module.loadStructuralLimitsFromCsv(kDefaultStructuralLimitsPath);
    if (!mass_loaded || !limits_loaded) {
        plugin_log(PLUGIN_LOG_ERROR, "Structures", "Failed to initialize default structural sources."
            " mass_loaded=" + std::to_string(mass_loaded)
            + " limits_loaded=" + std::to_string(limits_loaded));
        delete instance;
        return nullptr;
    }

    instance->initialized = true;

    return reinterpret_cast<PluginHandle>(instance);
}

/**
 * @brief Configures structural data paths from general configuration. (No actuators)
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

        // Mass/CoM/inertia are temporarily kept in JSON placeholders.
        const bool mass_loaded = instance->module.loadMassPropertiesFromJson(mass_json_path);
        const bool limits_loaded = instance->module.loadStructuralLimitsFromCsv(limits_csv_path);

        if (!mass_loaded || !limits_loaded) {
            if (!mass_loaded) {
                plugin_log(PLUGIN_LOG_ERROR, "Structures", "Failed to open JSON file: " + mass_json_path);
            }
            if (!limits_loaded) {
                plugin_log(PLUGIN_LOG_ERROR, "Structures", "Failed to open CSV file: " + limits_csv_path);
            }
            return -4;
        }

        return 0;
    } catch (...) {
        return -3;
    }
}

/**
 * @brief Executes a type-1 structural tick (state read-only, force/torque output).
 *
 * @warning This implementation intentionally does not apply active physics.
 * It returns neutral output (zeros) to preserve flow compatibility.
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
 * @brief Releases Structures plugin instance.
 */
PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (!handle) {
        return;
    }
    auto* instance = reinterpret_cast<StructuresPluginInstance*>(handle);
    delete instance;
}

} // extern "C"
