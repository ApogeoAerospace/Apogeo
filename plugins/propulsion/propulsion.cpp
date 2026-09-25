/** @file propulsion.cpp
 * @brief Propulsion C ABI adapter; validated data loading and neutral type-1 ticks.
 */
#include "plugin_api.h"
#include "propulsion_module.h"
#include "state_vector_generated.h"

#include <cmath>
#include <cstdio>
#include <nlohmann/json.hpp>

namespace {
constexpr const char* DEFAULT_CURVE_PATH = "data/propulsion/engine_curves.csv";
struct PropulsionPluginInstance {
    propulsion::PropulsionModule module;
};
} // namespace

extern "C" {
/** @brief Allocates an instance without file I/O; returns null on failure. */
PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    try {
        return reinterpret_cast<PluginHandle>(new PropulsionPluginInstance());
    } catch (...) { return nullptr; }
}

/** @brief Loads the configured CSV atomically; negative codes indicate failure. */
PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params) {
    if (!handle || !json_params) return -1;
    std::string path;
    try {
        const auto params = nlohmann::json::parse(json_params);
        if (!params.is_object()) return -3;
        path = params.value("engine_curves_path", std::string(DEFAULT_CURVE_PATH));
    } catch (...) { return -3; }
    try {
        reinterpret_cast<PropulsionPluginInstance*>(handle)->module.load_from_csv(path);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[Propulsion] %s\n", error.what());
        return -4;
    } catch (...) { return -4; }
}

/** @brief Verifies inputs and emits neutral outputs; never mutates host state. */
PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!data) return -1;
    auto* force = data->output_force ? data->output_force : data->force_out;
    auto* torque = data->output_torque ? data->output_torque : data->torque_out;
    if (force) *force = PluginVector3{};
    if (torque) *torque = PluginVector3{};
    if (!handle || !data->state_buffer) return -1;
    if (!reinterpret_cast<PropulsionPluginInstance*>(handle)->module.initialized()) return -2;
    if (!std::isfinite(data->delta_time) || data->delta_time < 0 ||
        data->buffer_size < sizeof(flatbuffers::uoffset_t) ||
        data->buffer_size >= FLATBUFFERS_MAX_BUFFER_SIZE) return -3;
    try {
        flatbuffers::Verifier verifier(data->state_buffer, data->buffer_size);
        if (!state_vector::VerifyGeneralStateBuffer(verifier)) return -3;
        return 0;
    } catch (...) { return -3; }
}

/** @brief Releases an instance; null is accepted. */
PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    delete reinterpret_cast<PropulsionPluginInstance*>(handle);
}
} // extern "C"
