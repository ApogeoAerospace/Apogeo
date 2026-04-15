#ifndef INITIAL_STATE_GENERATOR_H
#define INITIAL_STATE_GENERATOR_H

#include "flatbuffers/flatbuffers.h"
#include <string>

/**
 * @file InitialStateLoader.h
 * @brief Declarations for loading initial state from JSON into FlatBuffers.
 */

// Utilities to create initial simulation state from JSON.
namespace MoLab::InitialStateLoader {
    /**
     * @brief Builds a `GeneralState` from a JSON file.
     *
     * @param builder FlatBuffers builder where state is written.
     * @param filepath Input JSON file path.
     * @return `true` if state was created successfully.
     */
    bool create_state_from_json(flatbuffers::FlatBufferBuilder& builder, const std::string& filepath);
} // namespace MoLab::InitialStateLoader

#endif // INITIAL_STATE_GENERATOR_H
