#include "environment_module.h"
#include "plugin_api.h"

#include <string>

#include <nlohmann/json.hpp>

namespace {

constexpr const char* kRuntimeDefaultConfigPath = "data/default_config.json";
constexpr const char* kSourceDefaultConfigPath = "data/defaults/default_config.json";

struct EnvironmentPluginInstance {
    environment::EnvironmentModule module;
    bool initialized = false;
};

bool loadDefaultConfiguration(environment::EnvironmentModule& module) {
    return module.loadFromConfig(kRuntimeDefaultConfigPath)
        || module.loadFromConfig(kSourceDefaultConfigPath);
}

bool loadConfiguration(environment::EnvironmentModule& module, const std::string& config_path) {
    if (module.loadFromConfig(config_path)) {
        return true;
    }

    return config_path == kRuntimeDefaultConfigPath
        && module.loadFromConfig(kSourceDefaultConfigPath);
}

} // namespace

extern "C" {

PLUGIN_EXPORT void plugin_set_host_services(const PluginHostServices* services) {
    (void)services;
}

PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    auto* instance = new EnvironmentPluginInstance();
    instance->initialized = loadDefaultConfiguration(instance->module);
    return reinterpret_cast<PluginHandle>(instance);
}

PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params) {
    if (!handle || !json_params) {
        return -1;
    }

    auto* instance = reinterpret_cast<EnvironmentPluginInstance*>(handle);
    try {
        const nlohmann::json parameters = nlohmann::json::parse(json_params);
        const std::string config_path = parameters.value("config_path", kRuntimeDefaultConfigPath);
        instance->initialized = loadConfiguration(instance->module, config_path);
        return instance->initialized ? 0 : -2;
    } catch (...) {
        return -3;
    }
}

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data || !data->state_buffer) {
        return -1;
    }

    auto* instance = reinterpret_cast<EnvironmentPluginInstance*>(handle);
    if (!instance->initialized) {
        return -2;
    }

    PluginVector3* force_output = data->output_force ? data->output_force : data->force_out;
    PluginVector3* torque_output = data->output_torque ? data->output_torque : data->torque_out;
    if (!force_output) {
        return -1;
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
    delete reinterpret_cast<EnvironmentPluginInstance*>(handle);
}

} // extern "C"
