/** @file propulsion_module.cpp
 * @brief Table lookup and explicit placeholders for future physical calculations.
 */
#include "propulsion_module.h"
#include "propulsion_csv.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace propulsion {
void PropulsionModule::load_from_csv(const std::string& path) {
    curve_ = read_engine_curve_csv(path);
}

bool PropulsionModule::initialized() const noexcept { return !curve_.samples.empty(); }
const EngineCurve& PropulsionModule::curve() const noexcept { return curve_; }

std::optional<PerformanceSample> PropulsionModule::interpolate_performance(double pressure) const {
    if (!initialized() || !std::isfinite(pressure) ||
        pressure < curve_.samples.front().ambient_pressure_pa ||
        pressure > curve_.samples.back().ambient_pressure_pa) return std::nullopt;
    const auto upper = std::lower_bound(curve_.samples.begin(), curve_.samples.end(), pressure,
        [](const PerformanceSample& sample, double p) { return sample.ambient_pressure_pa < p; });
    if (upper->ambient_pressure_pa == pressure) return *upper;
    const auto& lower = *std::prev(upper);
    const double weight = (pressure - lower.ambient_pressure_pa) /
        (upper->ambient_pressure_pa - lower.ambient_pressure_pa);
    return PerformanceSample{pressure,
        lower.thrust_n + weight * (upper->thrust_n - lower.thrust_n),
        lower.isp_s + weight * (upper->isp_s - lower.isp_s)};
}

std::optional<double> PropulsionModule::compute_thrust_n(const EngineOperatingPoint&) const noexcept {
    return std::nullopt; // TODO: implement thermodynamic engine-cycle solver.
}
std::optional<double> PropulsionModule::compute_mass_flow_kg_s(const EngineOperatingPoint&) const noexcept {
    return std::nullopt; // TODO: implement propellant consumption without host-state mutation here.
}
} // namespace propulsion
