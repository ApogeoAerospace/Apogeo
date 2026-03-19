/*
 * Structures Plugin - Modulo de propiedades inerciales y limites fisicos.
 *
 * Este plugin mantiene una estructura similar a los plugins de aerodinamica
 * y propulsion: crea instancia, configura, procesa tick y destruye.
 *
 * Salida del plugin por tick: solo fuerza y torque.
 */

#include "../../src/api/plugin_api.h"
#include "state_vector_generated.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace {

constexpr double kStandardGravity = 9.80665;
constexpr double kEarthRadiusM = 6371000.0;
constexpr double kSeaLevelDensity = 1.225;
constexpr double kScaleHeight = 8400.0;

constexpr const char* kDefaultMassPropsPath = "data/mass/mass_properties.json";
constexpr const char* kDefaultStructuralLimitsPath = "data/limits/structural_limits.csv";

struct Vec3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct InertiaTensorData {
    double ixx = 0.0;
    double iyy = 0.0;
    double izz = 0.0;
    double ixy = 0.0;
    double ixz = 0.0;
    double iyz = 0.0;
};

struct ActuatorControl {
    std::string id;
    double max_rate_deg_s = 0.0;
    double range_deg = 0.0;
    double current_command_deg = 0.0; // Placeholder de control.
};

struct StructuralLimits {
    double max_g_load = 4.0;   
    double warning_g_load = 3.5;
    double max_dynamic_pressure = 50000.0;
    double warning_dynamic_pressure = 40000.0;
};

static std::vector<std::string> splitCsvLine(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        fields.push_back(item);
    }
    return fields;
}

class StructuresModule {
public:
    bool loadMassPropertiesFromJson(const std::string& json_path) {
        std::ifstream file(json_path);
        if (!file.is_open()) {
            std::cout << "[Structures] No se pudo abrir JSON: " << json_path << std::endl;
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

        if (j.contains("actuators") && j["actuators"].is_array()) {
            setActuatorsFromJsonArray(j["actuators"]);
        }

        return true;
    }

    void setActuatorsFromJsonArray(const nlohmann::json& actuator_array) {
        if (!actuator_array.is_array()) {
            return;
        }
        actuators_raw_.clear();
        for (const auto& actuator_json : actuator_array) {
            actuators_raw_.push_back(actuator_json);
        }
    }

    bool loadStructuralLimitsFromCsv(const std::string& csv_path) {
        std::ifstream file(csv_path);
        if (!file.is_open()) {
            std::cout << "[Structures] No se pudo abrir CSV: " << csv_path << std::endl;
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
                continue; // Header.
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

    void mapActuatorsPlaceholder() {
        actuators_.clear();
        for (const auto& raw : actuators_raw_) {
            ActuatorControl control;
            control.id = raw.value("id", "unknown");
            control.max_rate_deg_s = raw.value("max_rate_deg_s", 0.0);
            control.range_deg = raw.value("range_deg", 0.0);
            control.current_command_deg = 0.0;
            actuators_.push_back(control);
        }
    }

    Vec3d getCenterOfMass() const {
        return center_of_mass_;
    }

    InertiaTensorData getInertiaTensor() const {
        return inertia_tensor_;
    }

    bool checkStructuralIntegrity(double dynamic_pressure, double g_force) const {
        return dynamic_pressure <= limits_.max_dynamic_pressure && g_force <= limits_.max_g_load;
    }

    PluginVector3 computeStructuralForce(const state_vector::GeneralState* state) const {
        PluginVector3 out{0.0f, 0.0f, 0.0f};
        if (!state || !state->velocity()) {
            return out;
        }


        const double c = 5.0;
        out.x = static_cast<float>(-c * state->velocity()->x());
        out.y = static_cast<float>(-c * state->velocity()->y());
        out.z = static_cast<float>(-c * state->velocity()->z());
        return out;
    }

    PluginVector3 computeStructuralTorque(const state_vector::GeneralState* state) const {
        PluginVector3 out{0.0f, 0.0f, 0.0f};
        if (!state || !state->angular_velocity()) {
            return out;
        }

        
        const double c_ang = 3.0;
        out.x = static_cast<float>(-c_ang * state->angular_velocity()->x());
        out.y = static_cast<float>(-c_ang * state->angular_velocity()->y());
        out.z = static_cast<float>(-c_ang * state->angular_velocity()->z());
        return out;
    }

    double initialMassKg() const {
        return initial_total_mass_kg_;
    }

    double maxGLoad() const {
        return limits_.max_g_load;
    }

    double warningGLoad() const {
        return limits_.warning_g_load;
    }

private:
    double initial_total_mass_kg_ = 1000.0;
    double fuel_mass_kg_ = 0.0;
    Vec3d center_of_mass_{};
    InertiaTensorData inertia_tensor_{};
    StructuralLimits limits_{};
    std::vector<nlohmann::json> actuators_raw_;
    std::vector<ActuatorControl> actuators_;
};

struct StructuresPluginInstance {
    StructuresModule module;
    bool initialized = false;
    bool debug_output = false;
    double previous_speed_m_s = 0.0;
    bool speed_initialized = false;
};

} // namespace

extern "C" {

PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    auto* instance = new StructuresPluginInstance();

    instance->module.loadMassPropertiesFromJson(kDefaultMassPropsPath);
    instance->module.loadStructuralLimitsFromCsv(kDefaultStructuralLimitsPath);
    instance->module.mapActuatorsPlaceholder();
    instance->initialized = true;

    std::cout << "[Structures] Instancia creada." << std::endl;
    std::cout << "[Structures] Masa inicial (kg): " << instance->module.initialMassKg() << std::endl;
    std::cout << "[Structures] Limite G warning/max: "
              << instance->module.warningGLoad() << " / "
              << instance->module.maxGLoad() << std::endl;

    return reinterpret_cast<PluginHandle>(instance);
}

PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params) {
    if (!handle || !json_params) {
        return -1;
    }

    auto* instance = reinterpret_cast<StructuresPluginInstance*>(handle);
    if (!instance->initialized) {
        return -2;
    }

    try {
        const nlohmann::json params = nlohmann::json::parse(json_params);

        std::string mass_json_path = kDefaultMassPropsPath;
        std::string limits_csv_path = kDefaultStructuralLimitsPath;

        if (params.contains("mass_properties_path")) {
            mass_json_path = params["mass_properties_path"].get<std::string>();
        }
        if (params.contains("structural_limits_path")) {
            limits_csv_path = params["structural_limits_path"].get<std::string>();
        }
        if (params.contains("debug_output")) {
            instance->debug_output = params["debug_output"].get<bool>();
        }

        instance->module.loadMassPropertiesFromJson(mass_json_path);
        instance->module.loadStructuralLimitsFromCsv(limits_csv_path);
        if (params.contains("actuators")) {
            instance->module.setActuatorsFromJsonArray(params["actuators"]);
        }
        instance->module.mapActuatorsPlaceholder();
        return 0;
    } catch (...) {
        return -3;
    }
}

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data || !data->state_buffer) {
        return -1;
    }

    PluginVector3* force_output = data->output_force ? data->output_force : data->force_out;
    PluginVector3* torque_output = data->output_torque ? data->output_torque : data->torque_out;
    if (!force_output) {
        return -1;
    }

    auto* instance = reinterpret_cast<StructuresPluginInstance*>(handle);
    if (!instance->initialized) {
        return -2;
    }

    const auto* state = state_vector::GetGeneralState(data->state_buffer);
    if (!state || !state->position() || !state->velocity()) {
        return -3;
    }

    const double vx = state->velocity()->x();
    const double vy = state->velocity()->y();
    const double vz = state->velocity()->z();
    const double speed = std::sqrt(vx * vx + vy * vy + vz * vz);

    
    if (!instance->speed_initialized) {
        instance->previous_speed_m_s = speed;
        instance->speed_initialized = true;

        *force_output = instance->module.computeStructuralForce(state);
        if (torque_output) {
            *torque_output = instance->module.computeStructuralTorque(state);
        }
        return 0;
    }

    const double ax = data->delta_time > 0.0 ? (speed - instance->previous_speed_m_s) / data->delta_time : 0.0;
    instance->previous_speed_m_s = speed;
    const double g_force = std::abs(ax) / kStandardGravity;

    const double px = state->position()->x();
    const double py = state->position()->y();
    const double pz = state->position()->z();
    double altitude = std::sqrt(px * px + py * py + pz * pz) - kEarthRadiusM;
    altitude = std::max(0.0, altitude);

    const double air_density = kSeaLevelDensity * std::exp(-altitude / kScaleHeight);
    const double dynamic_pressure = 0.5 * air_density * speed * speed;

    const bool integrity_ok = instance->module.checkStructuralIntegrity(dynamic_pressure, g_force);
    if (!integrity_ok && instance->debug_output) {
        std::cout << "[Structures] Warning: limite estructural excedido. q="
                  << dynamic_pressure << " Pa, g=" << g_force << std::endl;
    }

    *force_output = instance->module.computeStructuralForce(state);

    if (torque_output) {
        *torque_output = instance->module.computeStructuralTorque(state);
    }

    return 0;
}

PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (!handle) {
        return;
    }
    auto* instance = reinterpret_cast<StructuresPluginInstance*>(handle);
    delete instance;
}

} // extern "C"
