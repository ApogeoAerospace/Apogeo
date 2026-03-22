#include "InitialStateLoader.h"
#include "flatbuffers/flatbuffers.h"
#include "state_vector_generated.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include "Logger.h"

/**
 * @file InitialStateLoader.cpp
 * @brief Implementación de la carga de estado inicial desde JSON a FlatBuffers.
 */

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string read_file_strip_bom(const fs::path& p)
{
    std::ifstream file(p, std::ios::binary);
    if (!file.is_open()) return {};

    std::ostringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();

    const std::string utf8_bom = "\xEF\xBB\xBF";
    if (content.compare(0, utf8_bom.size(), utf8_bom) == 0) {
        content.erase(0, utf8_bom.size());
    }
    return content;
}

static float get_float_fallback(const json& j, const std::string& key, float fallback = 0.0f)
{
    if (!j.contains(key)) return fallback;
    try {
        return static_cast<float>(j.at(key).get<double>());
    } catch (...) {
        return fallback;
    }
}

static state_vector::Vec3 to_vec3_from_obj(const json& obj)
{
    return state_vector::Vec3(
        static_cast<float>(obj.value("x", 0.0)),
        static_cast<float>(obj.value("y", 0.0)),
        static_cast<float>(obj.value("z", 0.0))
    );
}

static state_vector::Vec3 parse_vec3_any(const json& parent, const std::string& key)
{
    if (!parent.contains(key)) return state_vector::Vec3(0.0f, 0.0f, 0.0f);
    const auto& v = parent[key];
    try {
        if (v.is_array() && v.size() >= 3) {
            return state_vector::Vec3(
                static_cast<float>(v[0].get<double>()),
                static_cast<float>(v[1].get<double>()),
                static_cast<float>(v[2].get<double>())
            );
        } else if (v.is_object()) {
            return to_vec3_from_obj(v);
        }
    } catch (...) {}
    return state_vector::Vec3(0.0f, 0.0f, 0.0f);
}

static state_vector::Quaternion parse_quat_any(const json& parent, const std::string& key)
{
    if (!parent.contains(key)) return state_vector::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    const auto& q = parent[key];
    try {
        if (q.is_array() && q.size() >= 4) {
            return state_vector::Quaternion(
                static_cast<float>(q[0].get<double>()),
                static_cast<float>(q[1].get<double>()),
                static_cast<float>(q[2].get<double>()),
                static_cast<float>(q[3].get<double>())
            );
        } else if (q.is_object()) {
            return state_vector::Quaternion(
                static_cast<float>(q.value("x", 0.0)),
                static_cast<float>(q.value("y", 0.0)),
                static_cast<float>(q.value("z", 0.0)),
                static_cast<float>(q.value("w", 1.0))
            );
        }
    } catch (...) {}
    return state_vector::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
}

bool InitialStateLoader::create_state_from_json(flatbuffers::FlatBufferBuilder& builder, const std::string& filepath) {
    fs::path path = fs::u8path(filepath);

    if (!path.is_absolute()) {
        fs::path cwd = fs::current_path();
        if (fs::exists(cwd / path)) path = cwd / path;
    }

    std::string content = read_file_strip_bom(path);
    if (content.empty()) {
        LOG_ERROR(std::string("Unable to open or read initial state file: ") + path.string(), "InitialStateLoader");
        return false;
    }

    json data;
    try {
        data = json::parse(content);
    } catch (json::parse_error& e) {
        LOG_ERROR(std::string("Error parsing JSON in initial state file: ") + e.what(), "InitialStateLoader");
        return false;
    } catch (std::exception& e) {
        LOG_ERROR(std::string("Unexpected error while parsing JSON: ") + e.what(), "InitialStateLoader");
        return false;
    }

    // Scalars required by new schema
    float sim_time = get_float_fallback(data, "sim_time", 0.0f);
    float dt = get_float_fallback(data, "dt", 0.01f);

    float total_mass = get_float_fallback(data, "total_mass", 0.0f);

    float mach_number = get_float_fallback(data, "mach_number", 0.0f);
    float dynamic_pressure = get_float_fallback(data, "dynamic_pressure", 0.0f);
    float angle_of_attack = get_float_fallback(data, "angle_of_attack", 0.0f);
    float sideslip_angle = get_float_fallback(data, "sideslip_angle", 0.0f);

    float atm_density = get_float_fallback(data, "atm_density", 0.0f);
    float atm_pressure = get_float_fallback(data, "atm_pressure", 0.0f);
    float atm_temperature = get_float_fallback(data, "atm_temperature", 0.0f);

    // Structs
    auto position = parse_vec3_any(data, "position");
    auto velocity = parse_vec3_any(data, "velocity");
    auto orientation = parse_quat_any(data, "orientation");
    auto angular_velocity = parse_vec3_any(data, "angular_velocity");

    auto cg_location = parse_vec3_any(data, "cg_location");

    state_vector::InertiaTensor inertia_tensor(
        static_cast<float>(data.value("inertia_tensor", json::object()).value("ixx", 0.0)),
        static_cast<float>(data.value("inertia_tensor", json::object()).value("iyy", 0.0)),
        static_cast<float>(data.value("inertia_tensor", json::object()).value("izz", 0.0)),
        static_cast<float>(data.value("inertia_tensor", json::object()).value("ixy", 0.0)),
        static_cast<float>(data.value("inertia_tensor", json::object()).value("ixz", 0.0)),
        static_cast<float>(data.value("inertia_tensor", json::object()).value("iyz", 0.0))
    );

    auto wind_velocity = parse_vec3_any(data, "wind_velocity");
    auto gravity = parse_vec3_any(data, "gravity");

    // Arrays: propellant_masses
    std::vector<float> propellant_masses_vec;
    if (data.contains("propellant_masses") && data["propellant_masses"].is_array()) {
        for (const auto& pm : data["propellant_masses"]) {
            try { propellant_masses_vec.push_back(static_cast<float>(pm.get<double>())); } catch (...) {}
        }
    }
    auto propellant_masses_fb = propellant_masses_vec.empty()
        ? flatbuffers::Offset<flatbuffers::Vector<float>>()
        : builder.CreateVector(propellant_masses_vec);

    // Arrays: surface_deflections
    std::vector<float> surface_deflections_vec;
    if (data.contains("surface_deflections") && data["surface_deflections"].is_array()) {
        for (const auto& sd : data["surface_deflections"]) {
            try { surface_deflections_vec.push_back(static_cast<float>(sd.get<double>())); } catch (...) {}
        }
    }
    auto surface_deflections_fb = surface_deflections_vec.empty()
        ? flatbuffers::Offset<flatbuffers::Vector<float>>()
        : builder.CreateVector(surface_deflections_vec);

    // Arrays: engines (struct vector)
    flatbuffers::Offset<flatbuffers::Vector<const state_vector::EngineCmd*>> engines_fb;
    if (data.contains("engines") && data["engines"].is_array()) {
        std::vector<state_vector::EngineCmd> engines_vec;
        engines_vec.reserve(data["engines"].size());
        for (const auto& e : data["engines"]) {
            float throttle = 0.0f;
            state_vector::Vec3 tvc = state_vector::Vec3(0.0f, 0.0f, 0.0f);
            try {
                throttle = static_cast<float>(e.value("throttle", 0.0));
                if (e.contains("tvc_angles")) {
                    const auto& ta = e["tvc_angles"];
                    if (ta.is_array() && ta.size() >= 3) {
                        tvc = state_vector::Vec3(
                            static_cast<float>(ta[0].get<double>()),
                            static_cast<float>(ta[1].get<double>()),
                            static_cast<float>(ta[2].get<double>())
                        );
                    } else if (ta.is_object()) {
                        tvc = to_vec3_from_obj(ta);
                    }
                }
            } catch (...) {}
            engines_vec.emplace_back(throttle, tvc);
        }
        engines_fb = builder.CreateVectorOfStructs(engines_vec);
    }

    // Build table with all required fields
    state_vector::GeneralStateBuilder gs_builder(builder);
    gs_builder.add_sim_time(sim_time);
    gs_builder.add_dt(dt);

    gs_builder.add_position(&position);
    gs_builder.add_velocity(&velocity);
    gs_builder.add_orientation(&orientation);
    gs_builder.add_angular_velocity(&angular_velocity);

    gs_builder.add_total_mass(total_mass);
    gs_builder.add_cg_location(&cg_location);
    gs_builder.add_inertia_tensor(&inertia_tensor);

    if (propellant_masses_fb.o != 0) gs_builder.add_propellant_masses(propellant_masses_fb);

    gs_builder.add_mach_number(mach_number);
    gs_builder.add_dynamic_pressure(dynamic_pressure);
    gs_builder.add_angle_of_attack(angle_of_attack);
    gs_builder.add_sideslip_angle(sideslip_angle);

    gs_builder.add_atm_density(atm_density);
    gs_builder.add_atm_pressure(atm_pressure);
    gs_builder.add_atm_temperature(atm_temperature);

    gs_builder.add_wind_velocity(&wind_velocity);
    gs_builder.add_gravity(&gravity);

    if (engines_fb.o != 0) gs_builder.add_engines(engines_fb);
    if (surface_deflections_fb.o != 0) gs_builder.add_surface_deflections(surface_deflections_fb);

    auto general_state = gs_builder.Finish();
    builder.Finish(general_state);

    LOG_INFO(std::string("Initial state loaded successfully from: ") + path.string(), "InitialStateLoader");
    return true;
}
