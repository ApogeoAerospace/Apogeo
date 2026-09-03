#pragma once

#include "propulsion/interfaces.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace propulsion {

/**
 * @brief Liquid bi-propellant rocket model with throttling support.
 */
class LiquidRocketEngine final : public IEngineModel {
public:
    explicit LiquidRocketEngine(EngineConfig configuration);

    const EngineId& id() const noexcept override;
    EngineDemand evaluate(
        const EngineCommand& command,
        const EnvironmentState& environment,
        double delta_time_s) override;
    EngineOutput resolve(
        const EngineDemand& demand,
        const PropellantAllocation& allocation) override;
    EngineState state() const noexcept override;
    void reset() noexcept override;

private:
    EngineConfig configuration_;
    EngineState state_ = EngineState::off;
};

/**
 * @brief Solid rocket model with burn-curve-driven performance.
 */
class SolidRocketEngine final : public IEngineModel {
public:
    explicit SolidRocketEngine(EngineConfig configuration);

    const EngineId& id() const noexcept override;
    EngineDemand evaluate(
        const EngineCommand& command,
        const EnvironmentState& environment,
        double delta_time_s) override;
    EngineOutput resolve(
        const EngineDemand& demand,
        const PropellantAllocation& allocation) override;
    EngineState state() const noexcept override;
    void reset() noexcept override;

private:
    EngineConfig configuration_;
    EngineState state_ = EngineState::off;
    double burn_time_s_ = 0.0;
};

/**
 * @brief Data-driven engine model backed by an editable performance table.
 */
class TabulatedEngineModel final : public IEngineModel {
public:
    explicit TabulatedEngineModel(EngineConfig configuration);

    const EngineId& id() const noexcept override;
    EngineDemand evaluate(
        const EngineCommand& command,
        const EnvironmentState& environment,
        double delta_time_s) override;
    EngineOutput resolve(
        const EngineDemand& demand,
        const PropellantAllocation& allocation) override;
    EngineState state() const noexcept override;
    void reset() noexcept override;

private:
    EngineConfig configuration_;
    EngineState state_ = EngineState::off;
};

/**
 * @brief Default tank-based propellant storage and allocation policy.
 */
class TankPropellantSystem final : public IPropellantSystem {
public:
    Status configure(const std::vector<TankConfig>& tanks) override;
    std::vector<PropellantAllocation> allocate(
        const std::vector<EngineDemand>& demands,
        double delta_time_s) override;
    std::vector<TankState> snapshot() const override;
    void reset() noexcept override;

private:
    std::vector<TankConfig> configuration_;
    std::vector<TankState> tanks_;
};

/**
 * @brief Registry-based factory implementing the Open/Closed Principle.
 */
class EngineRegistry final : public IEngineFactory {
public:
    using Creator = std::function<std::unique_ptr<IEngineModel>(const EngineConfig&)>;

    EngineRegistry();

    bool registerModel(std::string model_type, Creator creator);
    std::unique_ptr<IEngineModel> create(
        const EngineConfig& configuration) const override;

private:
    std::unordered_map<std::string, Creator> creators_;
};

} // namespace propulsion
