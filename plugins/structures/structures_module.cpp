#include "structures_module.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace {

static std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        fields.push_back(item);
    }
    return fields;
}

static std::string resolveExistingPath(const std::string& input_path) {
    namespace fs = std::filesystem;
    std::error_code ec;

    fs::path original(input_path);
    if (original.is_absolute() && fs::exists(original, ec)) {
        return original.string();
    }

    std::vector<fs::path> candidates;
    candidates.push_back(original);

    const fs::path cwd = fs::current_path(ec);
    if (!ec && !cwd.empty()) {
        candidates.push_back(cwd / original);

        fs::path cursor = cwd;
        for (int i = 0; i < 6; ++i) {
            candidates.push_back(cursor / original);
            if (!cursor.has_parent_path()) {
                break;
            }
            cursor = cursor.parent_path();
        }
    }

    for (const auto& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }

        fs::path normalized = candidate.lexically_normal();
        if (fs::exists(normalized, ec)) {
            return normalized.string();
        }
    }

    return input_path;
}

} // namespace

namespace structures {

bool StructuresModule::loadMassPropertiesFromJson(const std::string& json_path) {
    const std::string resolved_path = resolveExistingPath(json_path);
    std::ifstream file(resolved_path);
    if (!file.is_open()) {
        return false;
    }

    nlohmann::json j;
    file >> j;

    initial_total_mass_kg_ = j.value("initial_total_mass_kg", initial_total_mass_kg_);
    fuel_mass_kg_ = j.value("fuel_mass_kg", fuel_mass_kg_);

    if (j.contains("center_of_mass_m") && j["center_of_mass_m"].is_object()) {
        const auto& cm = j["center_of_mass_m"];
        center_of_mass_.x = cm.value("x", center_of_mass_.x);
        center_of_mass_.y = cm.value("y", center_of_mass_.y);
        center_of_mass_.z = cm.value("z", center_of_mass_.z);
    }

    if (j.contains("inertia_tensor_kg_m2") && j["inertia_tensor_kg_m2"].is_object()) {
        const auto& it = j["inertia_tensor_kg_m2"];
        inertia_tensor_.ixx = it.value("ixx", inertia_tensor_.ixx);
        inertia_tensor_.iyy = it.value("iyy", inertia_tensor_.iyy);
        inertia_tensor_.izz = it.value("izz", inertia_tensor_.izz);
        inertia_tensor_.ixy = it.value("ixy", inertia_tensor_.ixy);
        inertia_tensor_.ixz = it.value("ixz", inertia_tensor_.ixz);
        inertia_tensor_.iyz = it.value("iyz", inertia_tensor_.iyz);
    }

    return true;
}

bool StructuresModule::loadStructuralLimitsFromCsv(const std::string& csv_path) {
    const std::string resolved_path = resolveExistingPath(csv_path);
    std::ifstream file(resolved_path);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    bool first_line = true;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        if (first_line) {
            first_line = false;
            continue;
        }

        const auto fields = splitCsvLine(line);
        if (fields.size() < 2) {
            continue;
        }

        const std::string& limit_name = fields[0];
        double value = 0.0;
        try {
            value = std::stod(fields[1]);
        } catch (...) {
            continue;
        }

        if (limit_name == "max_g_load") {
            limits_.max_g_load = value;
        } else if (limit_name == "warning_g_load") {
            limits_.warning_g_load = value;
        } else if (limit_name == "max_dynamic_pressure") {
            limits_.max_dynamic_pressure = value;
        } else if (limit_name == "warning_dynamic_pressure") {
            limits_.warning_dynamic_pressure = value;
        }
    }

    return true;
}

Vec3d StructuresModule::getCenterOfMass() const {
    return center_of_mass_;
}

InertiaTensorData StructuresModule::getInertiaTensor() const {
    return inertia_tensor_;
}

bool StructuresModule::checkStructuralIntegrity(double dynamic_pressure, double g_force) const {
    return dynamic_pressure <= limits_.max_dynamic_pressure && g_force <= limits_.max_g_load;
}

PluginVector3 StructuresModule::computeStructuralForce(const state_vector::GeneralState* state) const {
    PluginVector3 out{0.0f, 0.0f, 0.0f};
    if (!state || !state->velocity()) {
        return out;
    }

    // Modelo temporal de plantilla: amortiguamiento lineal simplificado.
    // Debe reemplazarse por el modelo estructural definitivo.
    const double c = 5.0;
    out.x = static_cast<float>(-c * state->velocity()->x());
    out.y = static_cast<float>(-c * state->velocity()->y());
    out.z = static_cast<float>(-c * state->velocity()->z());
    return out;
}

PluginVector3 StructuresModule::computeStructuralTorque(const state_vector::GeneralState* state) const {
    PluginVector3 out{0.0f, 0.0f, 0.0f};
    if (!state || !state->angular_velocity()) {
        return out;
    }

    // Modelo temporal de plantilla: amortiguamiento angular simplificado.
    // Debe reemplazarse por el modelo estructural definitivo.
    const double c_ang = 3.0;
    out.x = static_cast<float>(-c_ang * state->angular_velocity()->x());
    out.y = static_cast<float>(-c_ang * state->angular_velocity()->y());
    out.z = static_cast<float>(-c_ang * state->angular_velocity()->z());
    return out;
}

double StructuresModule::initialMassKg() const {
    return initial_total_mass_kg_;
}

double StructuresModule::maxGLoad() const {
    return limits_.max_g_load;
}

double StructuresModule::warningGLoad() const {
    return limits_.warning_g_load;
}

} // namespace structures
