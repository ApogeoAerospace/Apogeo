/*
 * Aerodynamics Plugin - database-backed skeleton.
 *
 * The current implementation only loads the aerodynamic database metadata.
 * Force and torque outputs remain neutral until coefficient interpolation and
 * aerodynamic force models are implemented.
 */

#include "aerodynamics_module.h"
#include "plugin_api.h"

#include <atomic>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

namespace {

constexpr const char* kRuntimeDefaultConfigPath = "data/default_config.json";
constexpr const char* kSourceDefaultConfigPath = "data/defaults/default_config.json";
constexpr uint32_t kSupportedHostServicesApiVersion = 1;

using HostLogFn = void(*)(int32_t, const char*, const char*, void*);

struct AerodynamicsPluginInstance {
    aerodynamics::AerodynamicsModule module;
    bool initialized = false;
    bool debug_output = false;
};

std::atomic<HostLogFn> g_host_log_fn{nullptr};
std::atomic<void*> g_host_user_data{nullptr};
std::atomic<bool> g_host_api_mismatch_warned{false};

void plugin_log(int32_t level, const char* component, const std::string& message) {
    HostLogFn host_log = g_host_log_fn.load(std::memory_order_acquire);
    if (host_log != nullptr) {
        void* user_data = g_host_user_data.load(std::memory_order_relaxed);
        host_log(level, component, message.c_str(), user_data);
        return;
    }

    std::cout << "[" << (component ? component : "Plugin") << "] " << message << std::endl;
}

bool loadDefaultConfig(aerodynamics::AerodynamicsModule& module) {
    return module.loadFromConfig(kRuntimeDefaultConfigPath)
        || module.loadFromConfig(kSourceDefaultConfigPath);
}

} // namespace

extern "C" {

PLUGIN_EXPORT void plugin_set_host_services(const PluginHostServices* services) {
    if (services != nullptr && services->log != nullptr) {
        if (services->api_version != kSupportedHostServicesApiVersion) {
            g_host_log_fn.store(nullptr, std::memory_order_release);
            g_host_user_data.store(nullptr, std::memory_order_relaxed);

            bool expected = false;
            if (g_host_api_mismatch_warned.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
                std::cout << "[Aerodynamics] WARNING: Host services api_version mismatch (host="
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

PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    auto* instance = new AerodynamicsPluginInstance();
    instance->initialized = loadDefaultConfig(instance->module);

    if (!instance->initialized) {
        plugin_log(PLUGIN_LOG_WARNING, "Aerodynamics", "Created without a loaded aerodynamic database.");
    }

    return reinterpret_cast<PluginHandle>(instance);
}

PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params) {
    if (!handle || !json_params) {
        return -1;
    }

    auto* instance = reinterpret_cast<AerodynamicsPluginInstance*>(handle);

    try {
        const nlohmann::json params = nlohmann::json::parse(json_params);
        if (params.contains("debug_output")) {
            instance->debug_output = params["debug_output"].get<bool>();
        }

        std::string config_path = kRuntimeDefaultConfigPath;
        if (params.contains("config_path")) {
            config_path = params["config_path"].get<std::string>();
        } else if (params.contains("aero_config_path")) {
            config_path = params["aero_config_path"].get<std::string>();
        }

        instance->initialized = instance->module.loadFromConfig(config_path);
        if (!instance->initialized) {
            plugin_log(PLUGIN_LOG_ERROR, "Aerodynamics", "Failed to load aerodynamic database from config: " + config_path);
            return -2;
        }

        if (instance->debug_output) {
            plugin_log(PLUGIN_LOG_INFO, "Aerodynamics", "Loaded aerodynamic database: " + instance->module.databasePath());
        }

        return 0;
    } catch (...) {
        return -3;
    }
}

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data || !data->state_buffer) {
        return -1;
    }

    PluginVector3* force_output = data->output_force ? data->output_force : data->force_out;
    PluginVector3* torque_output = data->output_torque ? data->output_torque : data->torque_out;
    if (!force_output) {
        return -1;
    }

    auto* instance = reinterpret_cast<AerodynamicsPluginInstance*>(handle);
    if (!instance->initialized) {
        return -2;
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

PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (!handle) {
        return;
    }

    auto* instance = reinterpret_cast<AerodynamicsPluginInstance*>(handle);
    delete instance;
}

} // extern "C"
