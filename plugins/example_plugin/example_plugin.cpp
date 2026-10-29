/*
 * Example Plugin - reference implementation of the plugin API.
 *
 * It is meant to be read and copied. Each block below shows one capability of the
 * simulator that a real module can use:
 *
 *  1. Lifecycle ............ create / configure / tick / destroy (plugin_api.h).
 *  2. Configuration ........ JSON parameters from `plugins[].parameters`, validated, applied
 *                            atomically (a bad configuration never leaves half a change).
 *  3. Host logging ......... `plugin_set_host_services` + fallback to std::cout when the
 *                            host logger is disabled (`use_host_logger`).
 *  4. Reading the state .... `GeneralState` FlatBuffer: mass, velocity, wind, density, gravity.
 *  5. Force output ......... parallel plugins (`"type": 1`) return force/torque; the core sums
 *                            them across plugins and integrates the motion.
 *  6. Gravity .............. the core only CARRIES `GeneralState.gravity`; a module turns it
 *                            into a force. With `apply_gravity` this plugin does exactly
 *                            that, which is how the Environment module is expected to work.
 *  7. State editing ........ sequential plugins (`"type": 0`) may mutate the state buffer
 *                            (`enable_state_nudge`). Parallel plugins must only read it.
 *  8. Telemetry ............ periodic INFO line and optional per-tick DEBUG output.
 *
 * Execution model (see docs/PLUGIN_API.md):
 *  - type 0 (sequential): runs one after another, may edit the state, gets NO force pointers.
 *  - type 1 (parallel):   runs concurrently, must not write the state, gets force pointers.
 * This plugin detects which mode it runs in from the force pointers, so the same binary
 * works for both.
 *
 * The force model itself is in example_physics.h (no external dependencies, unit tested).
 */

#include "plugin_api.h"
#include "example_physics.h"
#include "state_vector_generated.h"

#include <atomic>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

namespace {

namespace ex = example_plugin;

constexpr uint32_t kSupportedHostServicesApiVersion = 1;
constexpr const char* kComponent = "ExamplePlugin";

using HostLogFn = void(*)(int32_t, const char*, const char*, void*);

std::atomic<HostLogFn> g_host_log_fn{nullptr};
std::atomic<void*> g_host_user_data{nullptr};

/** @brief Per-instance data. The handle returned to the host points to one of these. */
struct ExamplePluginInstance {
    ex::Params params;
    uint64_t tick_count = 0;

    // Warn once per instance instead of once per tick.
    bool warned_nudge_in_parallel = false;
    bool warned_forces_in_sequential = false;
    bool warned_bad_state = false;
};

/**
 * @brief Logs through the host logger when enabled, otherwise to stdout.
 *
 * Going through the host keeps plugin messages in the same sink as the core (log file,
 * IPC `log` events, web panel). stdout is only a fallback for isolated runs.
 */
void plugin_log(int32_t level, const std::string& message) {
    HostLogFn host_log = g_host_log_fn.load(std::memory_order_acquire);
    if (host_log != nullptr) {
        host_log(level, kComponent, message.c_str(), g_host_user_data.load(std::memory_order_relaxed));
        return;
    }

    static const char* const names[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
    const int idx = (level >= 0 && level <= 4) ? level : 1;
    std::cout << "[" << kComponent << "][" << names[idx] << "] " << message << std::endl;
}

ex::Vec3d toVec(const state_vector::Vec3* v) {
    return v ? ex::Vec3d{v->x(), v->y(), v->z()} : ex::Vec3d{};
}

// --- Strict JSON readers: a wrong type is an error, not a silent default. ---

bool readBool(const nlohmann::json& json, const char* key, bool& out, std::string& error) {
    if (!json.contains(key)) {
        return true;
    }
    const auto& value = json[key];
    if (!value.is_boolean()) {
        error = std::string(key) + " must be a boolean";
        return false;
    }
    out = value.get<bool>();
    return true;
}

bool readNumber(const nlohmann::json& json, const char* key, double& out, std::string& error) {
    if (!json.contains(key)) {
        return true;
    }
    const auto& value = json[key];
    if (!value.is_number()) {
        error = std::string(key) + " must be a number";
        return false;
    }
    out = value.get<double>();
    return true;
}

bool readInteger(const nlohmann::json& json, const char* key, int64_t& out, std::string& error) {
    if (!json.contains(key)) {
        return true;
    }
    const auto& value = json[key];
    if (!value.is_number_integer()) {
        error = std::string(key) + " must be an integer";
        return false;
    }
    out = value.get<int64_t>();
    return true;
}

/** @brief Applies a JSON parameter object on top of `params` (unknown keys are ignored). */
bool applyJson(const nlohmann::json& json, ex::Params& params, std::string& error) {
    if (!json.is_object()) {
        error = "parameters must be a JSON object";
        return false;
    }

    // `use_host_logger` is consumed by the host (it decides whether to call
    // plugin_set_host_services), so it is deliberately not read here.
    return readBool(json, "debug_output", params.debug_output, error) &&
           readInteger(json, "log_interval_ticks", params.log_interval_ticks, error) &&
           readBool(json, "apply_gravity", params.apply_gravity, error) &&
           readNumber(json, "gravity_scale", params.gravity_scale, error) &&
           readNumber(json, "drag_coefficient", params.drag_coefficient, error) &&
           readNumber(json, "reference_area", params.reference_area, error) &&
           readBool(json, "enable_state_nudge", params.enable_state_nudge, error) &&
           readNumber(json, "step_size", params.step_size, error);
}

std::string describe(const ex::Params& p) {
    std::ostringstream out;
    out << "apply_gravity=" << (p.apply_gravity ? "true" : "false")
        << ", gravity_scale=" << p.gravity_scale
        << ", drag_coefficient=" << p.drag_coefficient
        << ", reference_area=" << p.reference_area
        << ", enable_state_nudge=" << (p.enable_state_nudge ? "true" : "false")
        << ", step_size=" << p.step_size
        << ", log_interval_ticks=" << p.log_interval_ticks
        << ", debug_output=" << (p.debug_output ? "true" : "false");
    return out.str();
}

} // namespace

extern "C" {

/**
 * @brief Receives optional host services (shared logger). Safe to ignore.
 */
PLUGIN_EXPORT void plugin_set_host_services(const PluginHostServices* services) {
    if (services != nullptr && services->log != nullptr &&
        services->api_version == kSupportedHostServicesApiVersion) {
        g_host_user_data.store(services->user_data, std::memory_order_relaxed);
        g_host_log_fn.store(services->log, std::memory_order_release);
        return;
    }

    // Missing services or an API version we do not understand: use local logging.
    g_host_log_fn.store(nullptr, std::memory_order_release);
    g_host_user_data.store(nullptr, std::memory_order_relaxed);
}

/**
 * @brief Creates an instance with inert defaults (no forces, no state changes).
 */
PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    auto* instance = new ExamplePluginInstance();
    plugin_log(PLUGIN_LOG_INFO, "Instance created");
    return reinterpret_cast<PluginHandle>(instance);
}

/**
 * @brief Applies `plugins[].parameters` (JSON). All-or-nothing.
 *
 * @return 0 on success, -1 invalid arguments, -2 malformed JSON, -3 invalid parameter.
 */
PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params) {
    if (!handle || !json_params) {
        return -1;
    }

    auto* instance = reinterpret_cast<ExamplePluginInstance*>(handle);

    nlohmann::json json;
    try {
        json = nlohmann::json::parse(json_params);
    } catch (const std::exception& e) {
        plugin_log(PLUGIN_LOG_ERROR, std::string("Invalid JSON parameters: ") + e.what());
        return -2;
    }

    // Work on a copy so that a rejected configuration leaves the instance untouched.
    ex::Params candidate = instance->params;
    std::string error;
    if (!applyJson(json, candidate, error) || !ex::validate(candidate, &error)) {
        plugin_log(PLUGIN_LOG_ERROR, "Configuration rejected: " + error);
        return -3;
    }

    instance->params = candidate;
    instance->warned_nudge_in_parallel = false;
    instance->warned_forces_in_sequential = false;
    plugin_log(PLUGIN_LOG_INFO, "Configured: " + describe(instance->params));
    return 0;
}

/**
 * @brief One simulation tick.
 *
 * @return 0 on success, -1 invalid arguments / unreadable state, -2 unusable state values.
 */
PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data || !data->state_buffer) {
        return -1;
    }

    auto* instance = reinterpret_cast<ExamplePluginInstance*>(handle);
    const ex::Params& params = instance->params;
    instance->tick_count++;

    // The host passes force pointers only to parallel plugins.
    PluginVector3* force_out = data->output_force ? data->output_force : data->force_out;
    PluginVector3* torque_out = data->output_torque ? data->output_torque : data->torque_out;
    const bool parallel = (force_out != nullptr);

    if (force_out) {
        *force_out = {0.0f, 0.0f, 0.0f};
    }
    if (torque_out) {
        *torque_out = {0.0f, 0.0f, 0.0f};
    }

    // 4. Read the state (const view: safe in both modes).
    const state_vector::GeneralState* state = state_vector::GetGeneralState(data->state_buffer);
    if (!state) {
        return -1;
    }

    ex::TickInput input;
    input.mass = state->total_mass();
    input.gravity = toVec(state->gravity());
    input.velocity = toVec(state->velocity());
    input.wind = toVec(state->wind_velocity());
    input.air_density = state->atm_density();

    // 7. State editing: sequential mode only.
    if (params.enable_state_nudge) {
        if (parallel) {
            if (!instance->warned_nudge_in_parallel) {
                instance->warned_nudge_in_parallel = true;
                plugin_log(PLUGIN_LOG_WARNING,
                           "enable_state_nudge ignored: parallel plugins must not write the state. "
                           "Use \"type\": 0 to try it.");
            }
        } else {
            auto* mutable_state = state_vector::GetMutableGeneralState(data->state_buffer);
            if (mutable_state && mutable_state->mutable_position()) {
                mutable_state->mutable_position()->mutate_x(
                    mutable_state->position()->x() + static_cast<float>(params.step_size));
            }
        }
    }

    // 5/6. Forces: only meaningful when the host gave us somewhere to put them.
    ex::Forces forces;
    if (parallel) {
        std::string error;
        if (!ex::computeForces(params, input, forces, &error)) {
            if (!instance->warned_bad_state) {
                instance->warned_bad_state = true;
                plugin_log(PLUGIN_LOG_WARNING, "Skipping forces: " + error);
            }
            return -2;
        }

        force_out->x = static_cast<float>(forces.total.x);
        force_out->y = static_cast<float>(forces.total.y);
        force_out->z = static_cast<float>(forces.total.z);
    } else if ((params.apply_gravity || params.drag_coefficient > 0.0) &&
               !instance->warned_forces_in_sequential) {
        instance->warned_forces_in_sequential = true;
        plugin_log(PLUGIN_LOG_WARNING,
                   "apply_gravity / drag_coefficient ignored: sequential plugins cannot output "
                   "forces. Use \"type\": 1 to apply them.");
    }

    // 8. Telemetry.
    const bool periodic = (instance->tick_count == 1) ||
                          (instance->tick_count % static_cast<uint64_t>(params.log_interval_ticks) == 0);
    if (params.debug_output || periodic) {
        const state_vector::Vec3* position = state->position();
        std::ostringstream line;
        line << std::fixed << std::setprecision(3)
             << "tick=" << instance->tick_count
             << " t=" << state->sim_time()
             << " mode=" << (parallel ? "parallel" : "sequential")
             << " pos=(" << (position ? position->x() : 0.0f) << ", "
             << (position ? position->y() : 0.0f) << ", "
             << (position ? position->z() : 0.0f) << ")"
             << " |v|=" << ex::norm(input.velocity)
             << " F_gravity=(" << forces.gravity.x << ", " << forces.gravity.y << ", " << forces.gravity.z << ")"
             << " F_drag=(" << forces.drag.x << ", " << forces.drag.y << ", " << forces.drag.z << ")";
        plugin_log(params.debug_output && !periodic ? PLUGIN_LOG_DEBUG : PLUGIN_LOG_INFO, line.str());
    }

    return 0;
}

/**
 * @brief Destroys the instance and releases its memory.
 */
PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (!handle) {
        return;
    }

    auto* instance = reinterpret_cast<ExamplePluginInstance*>(handle);
    plugin_log(PLUGIN_LOG_INFO, "Instance destroyed after " + std::to_string(instance->tick_count) + " ticks");
    delete instance;
}

} // extern "C"
