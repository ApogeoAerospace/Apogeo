#pragma once

#include "propulsion/interfaces.hpp"

#include <memory>
#include <vector>

namespace propulsion {

/**
 * @brief Application service coordinating engines, tanks, and aggregation.
 *
 * The module has no dependency on FlatBuffers, JSON, dynamic loading, or the
 * host logger. Those concerns belong to boundary adapters.
 */
class PropulsionModule final : public IPropulsionModule {
public:
    PropulsionModule(
        std::shared_ptr<const IEngineFactory> engine_factory,
        std::unique_ptr<IPropellantSystem> propellant_system);

    Status configure(const PropulsionConfig& configuration) override;
    Status tick(const TickInput& input, TickResult& output) override;
    void reset() noexcept override;
    PropulsionSnapshot snapshot() const override;

private:
    Status validateTickInput(const TickInput& input) const;

    std::shared_ptr<const IEngineFactory> engine_factory_;
    std::unique_ptr<IPropellantSystem> propellant_system_;
    std::vector<std::unique_ptr<IEngineModel>> engines_;
    PropulsionConfig configuration_;
    bool configured_ = false;
};

} // namespace propulsion
