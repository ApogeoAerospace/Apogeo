#pragma once

#include "plugin_api.h"
#include "propulsion/interfaces.hpp"

#include <memory>

namespace propulsion {

/**
 * @brief Converts the host FlatBuffer/tick ABI into typed propulsion values.
 */
class IStateMapper {
public:
    virtual ~IStateMapper() = default;

    virtual Status read(const PluginTickData& source, TickInput& target) const = 0;
    virtual Status write(const TickResult& source, PluginTickData& target) const = 0;
};

class FlatBufferStateMapper final : public IStateMapper {
public:
    Status read(const PluginTickData& source, TickInput& target) const override;
    Status write(const TickResult& source, PluginTickData& target) const override;
};

/**
 * @brief Boundary object owned by the opaque PluginHandle.
 *
 * It parses JSON configuration, maps FlatBuffers, invokes the typed domain
 * contract, and maps Status to the integer C ABI.
 */
class PluginAdapter final {
public:
    PluginAdapter(
        std::unique_ptr<IPropulsionModule> module,
        std::unique_ptr<IStateMapper> state_mapper);

    std::int32_t configure(const char* json_parameters) noexcept;
    std::int32_t tick(PluginTickData* data) noexcept;
    void setHostServices(const PluginHostServices* services) noexcept;

private:
    std::unique_ptr<IPropulsionModule> module_;
    std::unique_ptr<IStateMapper> state_mapper_;
    PluginHostServices host_services_{};
};

} // namespace propulsion

/**
 * @brief Public C ABI consumed by MoLab::PluginManager.
 *
 * These declarations intentionally match src/api/plugin_api.h. Keeping this
 * boundary free of C++ types preserves dynamic-loading compatibility.
 */
extern "C" {

PLUGIN_EXPORT PluginHandle plugin_create_instance();
PLUGIN_EXPORT int32_t plugin_configure(
    PluginHandle handle,
    const char* json_params);
PLUGIN_EXPORT int32_t plugin_tick(
    PluginHandle handle,
    PluginTickData* data);
PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle);
PLUGIN_EXPORT void plugin_set_host_services(
    const PluginHostServices* services);

} // extern "C"
