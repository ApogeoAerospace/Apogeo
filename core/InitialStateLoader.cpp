#include "InitialStateLoader.h"
#include "flatbuffers/flatbuffers.h"
#include "state_vector_generated.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

bool InitialStateLoader::create_state_from_json(flatbuffers::FlatBufferBuilder& builder, const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "ERROR: No se pudo abrir el archivo de estado: " << filepath << std::endl;
        return false;
    }

    json data;
    try {
        file >> data; // Parsea el contenido del archivo a un objeto JSON
    }
    catch (json::parse_error& e) {
        std::cerr << "ERROR: Error al parsear el JSON: " << e.what() << std::endl;
        return false;
    }

    // Extrae los datos del JSON y construye el FlatBuffer
    auto position = state_vector::Vec3(
        data["position"]["x"], data["position"]["y"], data["position"]["z"]);

    auto velocity = state_vector::Vec3(
        data["velocity"]["x"], data["velocity"]["y"], data["velocity"]["z"]);

    auto orientation = state_vector::Quaternion(
        data["orientation"]["x"], data["orientation"]["y"], data["orientation"]["z"], data["orientation"]["w"]);

    float atm_density = data["atm_density"];
    float atm_pressure = data["atm_pressure"];
    float atm_temperature = data["atm_temperature"];
    auto gravity = state_vector::Vec3(
        data["gravity"]["x"], data["gravity"]["y"], data["gravity"]["z"]);
    float utc = data["UTC"];
    float time = data["Time"];
    auto wind_speed = state_vector::Vec3(
        data["wind_speed"]["x"], data["wind_speed"]["y"], data["wind_speed"]["z"]);

    // Crea el estado general usando todos los argumentos requeridos
    auto general_state = state_vector::CreateGeneralState(
        builder,
        &position,
        &velocity,
        &orientation,
        atm_density,
        atm_pressure,
        atm_temperature,
        &gravity,
        utc,
        time,
        &wind_speed
    );

    // Finaliza el FlatBuffer
    builder.Finish(general_state);
    return true;
}
