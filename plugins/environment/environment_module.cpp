#include "environment_module.h"

#include <filesystem>
#include <fstream>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

std::string resolveExistingPath(const std::string& input_path) {
    namespace fs = std::filesystem;
    std::error_code error;

    const fs::path original(input_path);
    if (original.is_absolute() && fs::exists(original, error)) {
        return original.string();
    }

    std::vector<fs::path> candidates{original};
    const fs::path working_directory = fs::current_path(error);
    if (!error && !working_directory.empty()) {
        fs::path current = working_directory;
        for (int depth = 0; depth < 7; ++depth) {
            candidates.push_back(current / original);
            if (!current.has_parent_path()) {
                break;
            }
            current = current.parent_path();
        }
    }

    for (const fs::path& candidate : candidates) {
        const fs::path normalized = candidate.lexically_normal();
        if (fs::exists(normalized, error)) {
            return normalized.string();
        }
    }

    return input_path;
}

class Wgs84PlanetModel final : public environment::PlanetModel {
public:
    const std::string& name() const override {
        static const std::string kName = "WGS84";
        return kName;
    }

    environment::Vector3d getGravityVector(const environment::Vector3d& position_m) const override {
        (void)position_m;
        return {};
    }
};

class UsStandard1976AtmosphereModel final : public environment::AtmosphereModel {
public:
    const std::string& name() const override {
        static const std::string kName = "US_Standard_1976";
        return kName;
    }

    double getDensityAtAltitude(double altitude_m) const override {
        (void)altitude_m;
        return 0.0;
    }
};

environment::SourceDefinition readSource(const nlohmann::json& source) {
    return {
        source.value("uri", ""),
        source.value("format", "")
    };
}

} // namespace

namespace environment {

bool EnvironmentModule::loadFromConfig(const std::string& config_path) {
    loaded_ = false;
    planet_model_.reset();
    atmosphere_model_.reset();
    configuration_ = {};

    std::ifstream config_file(resolveExistingPath(config_path));
    if (!config_file.is_open()) {
        return false;
    }

    try {
        nlohmann::json config;
        config_file >> config;

        const nlohmann::json& mission = config.at("mission_environment");
        EnvironmentConfiguration parsed_configuration;
        parsed_configuration.planet_model = mission.at("planet_model").get<std::string>();
        parsed_configuration.flight_sequence = readSource(mission.at("flight_sequence").at("source"));

        const nlohmann::json& atmosphere_wind = mission.at("atmosphere_wind");
        parsed_configuration.atmosphere_model = atmosphere_wind.at("model").get<std::string>();
        parsed_configuration.wind_profile = readSource(atmosphere_wind.at("wind_profile_source"));

        std::unique_ptr<PlanetModel> planet_model = createPlanetModel(parsed_configuration.planet_model);
        std::unique_ptr<AtmosphereModel> atmosphere_model = createAtmosphereModel(parsed_configuration.atmosphere_model);
        if (!planet_model || !atmosphere_model) {
            return false;
        }

        configuration_ = std::move(parsed_configuration);
        planet_model_ = std::move(planet_model);
        atmosphere_model_ = std::move(atmosphere_model);
        loaded_ = true;
        return true;
    } catch (...) {
        return false;
    }
}

bool EnvironmentModule::isLoaded() const {
    return loaded_;
}

const EnvironmentConfiguration& EnvironmentModule::configuration() const {
    return configuration_;
}

const PlanetModel* EnvironmentModule::planetModel() const {
    return planet_model_.get();
}

const AtmosphereModel* EnvironmentModule::atmosphereModel() const {
    return atmosphere_model_.get();
}

Vector3d EnvironmentModule::getGravityVector(const Vector3d& position_m) const {
    if (!planet_model_) {
        return {};
    }

    return planet_model_->getGravityVector(position_m);
}

double EnvironmentModule::getAtmosphericDensity(double altitude_m) const {
    if (!atmosphere_model_) {
        return 0.0;
    }

    return atmosphere_model_->getDensityAtAltitude(altitude_m);
}

std::unique_ptr<PlanetModel> EnvironmentModule::createPlanetModel(const std::string& model_name) {
    if (model_name == "WGS84") {
        return std::make_unique<Wgs84PlanetModel>();
    }

    return nullptr;
}

std::unique_ptr<AtmosphereModel> EnvironmentModule::createAtmosphereModel(const std::string& model_name) {
    if (model_name == "US_Standard_1976") {
        return std::make_unique<UsStandard1976AtmosphereModel>();
    }

    return nullptr;
}

} // namespace environment
