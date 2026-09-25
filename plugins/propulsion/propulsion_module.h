/** @file propulsion_module.h
 * @brief In-memory propulsion data and future engine-cycle interface.
 */
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace propulsion {

enum class InterpolationMethod { Linear };

/** @brief One pressure node; SI units are encoded in member names. */
struct PerformanceSample {
    double ambient_pressure_pa = 0.0;
    double thrust_n = 0.0;
    double isp_s = 0.0;
};

/** @brief A pressure curve at fixed chamber pressure and oxidizer/fuel mass ratio. */
struct EngineCurve {
    double chamber_pressure_pa = 0.0;
    double mixture_ratio = 0.0;
    InterpolationMethod interpolation = InterpolationMethod::Linear;
    std::vector<PerformanceSample> samples;
};

/** @brief Inputs reserved for the future engine-cycle solver. */
struct EngineOperatingPoint {
    double ambient_pressure_pa = 0.0;
    double throttle = 0.0;
};

/** @brief Owns validated data independently of the host ABI and future physics. */
class PropulsionModule {
public:
    /** @brief Atomically loads a curve. @param path CSV path relative to cwd or absolute.
     * @throws std::runtime_error Invalid/unreadable CSV; previous data are preserved.
     */
    void load_from_csv(const std::string& path);
    /** @brief Returns whether a complete validated curve has been loaded. */
    bool initialized() const noexcept;
    /** @brief Returns read-only data, valid until successful reload or destruction. */
    const EngineCurve& curve() const noexcept;
    /** @brief Interpolates tabulated data, not physical engine-cycle output.
     * @param ambient_pressure_pa Pressure in Pa.
     * @return Empty for uninitialized data, nonfinite input or out-of-domain pressure.
     */
    std::optional<PerformanceSample> interpolate_performance(double ambient_pressure_pa) const;
    /** @brief Future thrust solver. @param point Operating inputs. @return Empty: not implemented. */
    std::optional<double> compute_thrust_n(const EngineOperatingPoint& point) const noexcept;
    /** @brief Future consumption solver. @param point Operating inputs. @return Empty: not implemented. */
    std::optional<double> compute_mass_flow_kg_s(const EngineOperatingPoint& point) const noexcept;

private:
    EngineCurve curve_;
};

} // namespace propulsion
