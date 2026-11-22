#pragma once

#include <memory>
#include <string>

namespace environment {

struct Vector3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct SourceDefinition {
    std::string uri;
    std::string format;
};

struct EnvironmentConfiguration {
    std::string planet_model;
    std::string atmosphere_model;
    SourceDefinition flight_sequence;
    SourceDefinition wind_profile;
};

/**
 * @brief Contract for a planetary model selected from mission configuration.
 */
class PlanetModel {
public:
    virtual ~PlanetModel() = default;

    virtual const std::string& name() const = 0;

    /**
     * @brief Future gravity-vector calculation interface.
     */
    virtual Vector3d getGravityVector(const Vector3d& position_m) const = 0;
};

/**
 * @brief Contract for an atmospheric and wind model selected from configuration.
 */
class AtmosphereModel {
public:
    virtual ~AtmosphereModel() = default;

    virtual const std::string& name() const = 0;

    /**
     * @brief Future atmospheric-density calculation interface.
     */
    virtual double getDensityAtAltitude(double altitude_m) const = 0;
};

class EnvironmentModule {
public:
    /**
     * @brief Parses mission_environment from the general simulator JSON file.
     */
    bool loadFromConfig(const std::string& config_path);

    bool isLoaded() const;
    const EnvironmentConfiguration& configuration() const;
    const PlanetModel* planetModel() const;
    const AtmosphereModel* atmosphereModel() const;

    /**
     * @brief Delegates the future gravity calculation to the selected planet model.
     */
    Vector3d getGravityVector(const Vector3d& position_m) const;

    /**
     * @brief Delegates the future density calculation to the selected atmosphere model.
     */
    double getAtmosphericDensity(double altitude_m) const;

private:
    static std::unique_ptr<PlanetModel> createPlanetModel(const std::string& model_name);
    static std::unique_ptr<AtmosphereModel> createAtmosphereModel(const std::string& model_name);

    EnvironmentConfiguration configuration_{};
    std::unique_ptr<PlanetModel> planet_model_;
    std::unique_ptr<AtmosphereModel> atmosphere_model_;
    bool loaded_ = false;
};

} // namespace environment
