#pragma once

#include <string>
#include <sol/sol.hpp>
#include "state_vector_generated.h"
#include "flatbuffers/flatbuffers.h"

/**
 * @file FlightComputerContext.h
 * @brief Declaration of the embedded Lua flight computer context.
 */

namespace Apogeo {

/**
 * @class FlightComputerContext
 * @brief Manages a Lua interpreter for user-written flight scripts.
 *
 * Loads and executes a Lua script each simulation tick, exposing
 * read-only state accessors and write command injectors through
 * an API bridge.
 */
class FlightComputerContext {
public:
    /**
     * @brief Constructs context and loads the given Lua script.
     * @param script_path Filesystem path to the Lua script file.
     */
    explicit FlightComputerContext(const std::string& script_path);

    /**
     * @brief Executes the script's on_tick function and rebuilds the state buffer.
     *
     * @param in      Current simulation state (read-only).
     * @param fbb     FlatBufferBuilder used to produce the updated state.
     * @param dt      Simulation time step in seconds.
     */
    void update(const state_vector::GeneralState* in,
                flatbuffers::FlatBufferBuilder& fbb,
                float dt);

    /**
     * @brief Checks whether the script loaded without errors.
     * @return `true` if the script is ready for execution.
     */
    bool is_valid() const;

    /**
     * @brief Serializes user-defined global primitives (number, boolean, string) to a JSON string.
     * @return JSON string representing persistent Lua globals.
     */
    std::string save_lua_globals() const;

    /**
     * @brief Restores global variables into the Lua environment from a JSON string.
     * @param json_str JSON string containing key/value primitive pairs.
     * @return true on success, false if JSON parsing fails or unsupported types are encountered.
     */
    bool restore_lua_globals(const std::string& json_str);

private:
    /**
     * @brief Registers C++ functions into the Lua environment.
     */
    void register_api();

    sol::state lua_;
    bool valid_ = false;

    // Internal variables read/written by API bridge lambdas.
    float current_altitude_  = 0.0f;
    float current_vertical_velocity_ = 0.0f;
    float throttle_cmd_      = 0.0f;
};

} // namespace Apogeo
