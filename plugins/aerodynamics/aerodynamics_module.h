#pragma once

#include "state_vector_generated.h"

#include <map>
#include <string>
#include <vector>

namespace aerodynamics {

struct AeroAxis {
    std::string name;
    std::vector<double> values;
};

struct AeroCoefficients {
    double cx = 0.0;
    double cy = 0.0;
    double cz = 0.0;
    double cl = 0.0;
    double cm = 0.0;
    double cn = 0.0;
};

/**
 * @brief Aerodynamic database reader for HDF5 lookup tables.
 *
 * The module reads the configured axis order from JSON and loads the matching
 * datasets from an HDF5 database. Interpolation is intentionally left as a
 * placeholder for the next aerodynamic-model task.
 */
class AerodynamicsModule {
public:
    bool loadFromConfig(const std::string& config_path);
    bool loadDatabase(const std::string& database_path, const std::vector<std::string>& axis_names);

    bool isLoaded() const;
    const std::string& databasePath() const;
    const std::vector<AeroAxis>& axes() const;
    const std::map<std::string, std::vector<unsigned long long>>& coefficientDimensions() const;

    /**
     * @brief Future interpolation entry point.
     *
     * @todo Use the state vector to interpolate coefficient datasets over the
     * configured aerodynamic axes.
     */
    AeroCoefficients getCoefficients(const state_vector::GeneralState* state_vector) const;

private:
    std::string database_path_;
    std::vector<AeroAxis> axes_;
    std::map<std::string, std::vector<unsigned long long>> coefficient_dimensions_;
    bool loaded_ = false;
};

} // namespace aerodynamics
