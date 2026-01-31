#ifndef INITIAL_STATE_GENERATOR_H
#define INITIAL_STATE_GENERATOR_H

#include "flatbuffers/flatbuffers.h"

// Utilidades para crear el estado inicial de simulación desde JSON.
namespace InitialStateLoader {
    /**
     * Rellena un FlatBufferBuilder con un estado de vehículo.
     * @param builder: El constructor de FlatBuffers que se rellenará con los datos.
     */
    bool create_state_from_json(flatbuffers::FlatBufferBuilder& builder, const std::string& filepath);
}

#endif // INITIAL_STATE_GENERATOR_H
