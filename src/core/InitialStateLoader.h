#ifndef INITIAL_STATE_GENERATOR_H
#define INITIAL_STATE_GENERATOR_H

#include "flatbuffers/flatbuffers.h"

/**
 * @file InitialStateLoader.h
 * @brief Declaraciones para cargar el estado inicial desde JSON hacia FlatBuffers.
 */

// Utilidades para crear el estado inicial de simulación desde JSON.
namespace InitialStateLoader {
    /**
     * @brief Construye un `GeneralState` a partir de un archivo JSON.
     *
     * @param builder Constructor de FlatBuffers donde se escribirá el estado.
     * @param filepath Ruta del archivo JSON de entrada.
     * @return `true` si el estado fue creado correctamente.
     */
    bool create_state_from_json(flatbuffers::FlatBufferBuilder& builder, const std::string& filepath);
}

#endif // INITIAL_STATE_GENERATOR_H
