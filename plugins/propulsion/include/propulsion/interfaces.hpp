#pragma once

#include "propulsion/types.hpp"

#include <memory>
#include <vector>

namespace propulsion {

/**
 * @brief Polymorphic engine performance model.
 *
 * Evaluation and resolution are separated so all engines can request
 * propellant first and receive a deterministic, atomic tank allocation.
 */
class IEngineModel {
public:
    virtual ~IEngineModel() = default;

    virtual const EngineId& id() const noexcept = 0;
    virtual EngineDemand evaluate(
        const EngineCommand& command,
        const EnvironmentState& environment,
        double delta_time_s) = 0;
    virtual EngineOutput resolve(
        const EngineDemand& demand,
        const PropellantAllocation& allocation) = 0;
    virtual EngineState state() const noexcept = 0;
    virtual void reset() noexcept = 0;
};

/**
 * @brief Owns tank state and allocates propellant to all engines atomically.
 */
class IPropellantSystem {
public:
    virtual ~IPropellantSystem() = default;

    virtual Status configure(const std::vector<TankConfig>& tanks) = 0;
    virtual std::vector<PropellantAllocation> allocate(
        const std::vector<EngineDemand>& demands,
        double delta_time_s) = 0;
    virtual std::vector<TankState> snapshot() const = 0;
    virtual void reset() noexcept = 0;
};

/**
 * @brief Extensible engine construction contract.
 *
 * New models are registered under a model_type without changing
 * PropulsionModule.
 */
class IEngineFactory {
public:
    virtual ~IEngineFactory() = default;

    virtual std::unique_ptr<IEngineModel> create(
        const EngineConfig& configuration) const = 0;
};

/**
 * @brief Main typed contract of the propulsion domain.
 *
 * PluginAdapter exposes this contract through the repository C plugin ABI.
 */
class IPropulsionModule {
public:
    virtual ~IPropulsionModule() = default;

    virtual Status configure(const PropulsionConfig& configuration) = 0;
    virtual Status tick(const TickInput& input, TickResult& output) = 0;
    virtual void reset() noexcept = 0;
    virtual PropulsionSnapshot snapshot() const = 0;
};

} // namespace propulsion
