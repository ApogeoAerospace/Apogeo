#include "InitialStateLoader.h"
#include "flatbuffers/flatbuffers.h"
#include "state_vector_generated.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include "Logger.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string read_file_strip_bom(const fs::path& p)
{
    std::ifstream file(p, std::ios::binary);
    if (!file.is_open()) return {};

    std::ostringstream ss;
    ss << file.rdbuf();     
    std::string content = ss.str();

    // Eliminar BOM UTF-8 si está presente
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
        return j.value(key, fallback);
    } catch (...) {
        return fallback;
    }
}

static state_vector::Vec3 parse_vec3(const json& parent, const std::string& key)
{
    if (!parent.contains(key)) return state_vector::Vec3(0.0f, 0.0f, 0.0f);
    const auto& v = parent[key];
    try {
        if (v.is_array() && v.size() >= 3) {
            return state_vector::Vec3(
                static_cast<float>(v[0].get<double>()),
                static_cast<float>(v[1].get<double>()),
                static_cast<float>(v[2].get<double>()));
        } else if (v.is_object()) {
            return state_vector::Vec3(
                static_cast<float>(v.value("x", 0.0)),
                static_cast<float>(v.value("y", 0.0)),
                static_cast<float>(v.value("z", 0.0)));
        }
    } catch (...) {
        // continuar hasta el valor por defecto
    }
    return state_vector::Vec3(0.0f, 0.0f, 0.0f);
}

static float parse_time_value(const json& j, const std::string& primary, const std::string& alt, float fallback = 0.0f)
{
    if (j.contains(primary)) {
        try { return static_cast<float>(j.value(primary, fallback)); } catch (...) {}
    }
    if (j.contains(alt)) {
        try { return static_cast<float>(j.value(alt, fallback)); } catch (...) {}
    }
    return fallback;
}

bool InitialStateLoader::create_state_from_json(flatbuffers::FlatBufferBuilder& builder, const std::string& filepath) {
    fs::path path = fs::u8path(filepath);

    // Intentar varias ubicaciones razonables si la ruta es relativa
    if (!path.is_absolute()) {
        fs::path cwd = fs::current_path();
        if (fs::exists(cwd / path)) path = cwd / path;
    }

    std::string content = read_file_strip_bom(path);
    if (content.empty()) {
        LOG_ERROR(std::string("No se puede abrir o leer el archivo de estado inicial: ") + path.string(), "InitialStateLoader");
        return false;
    }

    json data;
    try {
        data = json::parse(content);
    } catch (json::parse_error& e) {
        LOG_ERROR(std::string("Error al parsear JSON en el archivo de estado inicial: ") + e.what(), "InitialStateLoader");
        return false;
    } catch (std::exception& e) {
        LOG_ERROR(std::string("Error inesperado al parsear JSON: ") + e.what(), "InitialStateLoader");
        return false;
    }

    // Analizar campos vectoriales con manejo tolerante (objeto o arreglo)
    auto position = parse_vec3(data, "position");
    auto velocity = parse_vec3(data, "velocity");
    auto orientation = state_vector::Quaternion(
        static_cast<float>(data.value("orientation", json::object()).value("x", 0.0)),
        static_cast<float>(data.value("orientation", json::object()).value("y", 0.0)),
        static_cast<float>(data.value("orientation", json::object()).value("z", 0.0)),
        static_cast<float>(data.value("orientation", json::object()).value("w", 1.0))
    );

    float atm_density = get_float_fallback(data, "atm_density", 0.0f);
    float atm_pressure = get_float_fallback(data, "atm_pressure", 0.0f);
    float atm_temperature = get_float_fallback(data, "atm_temperature", 0.0f);

    auto gravity = parse_vec3(data, "gravity");

    // Aceptar "Time" o "time", "UTC" o "utc"
    float time = parse_time_value(data, "Time", "time", 0.0f);
    float utc = parse_time_value(data, "UTC", "utc", 0.0f);

    auto wind_speed = parse_vec3(data, "wind_speed");

    // Crear el estado general usando los parámetros requeridos
    auto general_state = state_vector::CreateGeneralState(
        builder,
        &position,
        &velocity,
        &orientation,
        atm_density,
        atm_pressure,
        atm_temperature,
        &gravity,
        time,
        utc,
        &wind_speed
    );

    // Finaliza el FlatBuffer
    builder.Finish(general_state);

    LOG_INFO(std::string("Estado inicial cargado correctamente desde: ") + path.string(), "InitialStateLoader");
    return true;
}
