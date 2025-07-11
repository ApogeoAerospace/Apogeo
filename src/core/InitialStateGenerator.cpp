#include "InitialStateGenerator.h"
#include "flatbuffers/flatbuffers.h"
#include "state_vector_generated.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

bool InitialStateGenerator::create_state_from_json(flatbuffers::FlatBufferBuilder& builder, const std::string& filepath) {
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

    auto vehicle_state = state_vector::CreateVehicleState(builder,
        &position,
        &velocity,
        &orientation,
        data["fuel_percentage"]);

    builder.Finish(vehicle_state);
    return true;
}
