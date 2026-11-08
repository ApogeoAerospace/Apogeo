#include "FlightComputerContext.h"
#include "Logger.h"
#include <algorithm>
#include <unordered_set>
#include <nlohmann/json.hpp>

/**
 * @file FlightComputerContext.cpp
 * @brief Implementation of the embedded Lua flight computer context.
 */

namespace Apogeo {

namespace {

nlohmann::json sol_table_to_json(const sol::table& tbl, std::unordered_set<const void*>& visited) {
    const void* ptr = tbl.pointer();
    if (visited.find(ptr) != visited.end()) {
        return nullptr; // Cycle detected: omit to prevent infinite recursion
    }
    visited.insert(ptr);

    // Determine if table is a contiguous 1-based sequential array
    size_t count = 0;
    for (const auto& pair : tbl) {
        (void)pair;
        ++count;
    }

    bool is_array = false;
    if (count > 0) {
        bool all_int_keys = true;
        for (const auto& pair : tbl) {
            if (!pair.first.is<int>()) {
                all_int_keys = false;
                break;
            }
            int idx = pair.first.as<int>();
            if (idx < 1 || static_cast<size_t>(idx) > count) {
                all_int_keys = false;
                break;
            }
        }
        if (all_int_keys) {
            bool contiguous = true;
            for (size_t i = 1; i <= count; ++i) {
                sol::object item = tbl[i];
                if (!item.valid() || item.get_type() == sol::type::lua_nil) {
                    contiguous = false;
                    break;
                }
            }
            if (contiguous) {
                is_array = true;
            }
        }
    }

    nlohmann::json result;
    if (is_array) {
        result = nlohmann::json::array();
        for (size_t i = 1; i <= count; ++i) {
            sol::object item = tbl[i];
            sol::type t = item.get_type();
            if (t == sol::type::number) {
                result.push_back(item.as<double>());
            } else if (t == sol::type::boolean) {
                result.push_back(item.as<bool>());
            } else if (t == sol::type::string) {
                result.push_back(item.as<std::string>());
            } else if (t == sol::type::table) {
                auto child = sol_table_to_json(item.as<sol::table>(), visited);
                if (!child.is_null()) {
                    result.push_back(child);
                }
            }
        }
    } else {
        result = nlohmann::json::object();
        for (const auto& pair : tbl) {
            std::string key;
            if (pair.first.is<std::string>()) {
                key = pair.first.as<std::string>();
            } else if (pair.first.is<int>()) {
                key = std::to_string(pair.first.as<int>());
            } else {
                continue; // Skip non-string / non-int keys
            }

            sol::type t = pair.second.get_type();
            if (t == sol::type::number) {
                result[key] = pair.second.as<double>();
            } else if (t == sol::type::boolean) {
                result[key] = pair.second.as<bool>();
            } else if (t == sol::type::string) {
                result[key] = pair.second.as<std::string>();
            } else if (t == sol::type::table) {
                auto child = sol_table_to_json(pair.second.as<sol::table>(), visited);
                if (!child.is_null()) {
                    result[key] = child;
                }
            }
        }
    }

    visited.erase(ptr);
    return result;
}

bool is_valid_json_lua_type(const nlohmann::json& v) {
    if (v.is_number() || v.is_boolean() || v.is_string()) {
        return true;
    }
    if (v.is_array()) {
        for (const auto& item : v) {
            if (!is_valid_json_lua_type(item)) {
                return false;
            }
        }
        return true;
    }
    if (v.is_object()) {
        for (auto it = v.begin(); it != v.end(); ++it) {
            if (!is_valid_json_lua_type(it.value())) {
                return false;
            }
        }
        return true;
    }
    return false;
}

sol::object json_to_sol(sol::state_view lua, const nlohmann::json& val) {
    if (val.is_number()) {
        return sol::make_object(lua, val.get<double>());
    } else if (val.is_boolean()) {
        return sol::make_object(lua, val.get<bool>());
    } else if (val.is_string()) {
        return sol::make_object(lua, val.get<std::string>());
    } else if (val.is_array()) {
        sol::table tbl = lua.create_table();
        int idx = 1;
        for (const auto& item : val) {
            tbl[idx++] = json_to_sol(lua, item);
        }
        return tbl;
    } else if (val.is_object()) {
        sol::table tbl = lua.create_table();
        for (auto it = val.begin(); it != val.end(); ++it) {
            tbl[it.key()] = json_to_sol(lua, it.value());
        }
        return tbl;
    }
    return sol::make_object(lua, sol::nil);
}

} // namespace

FlightComputerContext::FlightComputerContext(const std::string& script_path) {
    // ==========================================
    // PHASE 1: SCRIPT ENVIRONMENT INITIALIZATION
    // ==========================================

    // Layer 1: Sandbox - Load only safe base and math libraries
    lua_.open_libraries(sol::lib::base, sol::lib::math);

    // Layer 1: Security - Block dangerous functions that could execute arbitrary files
    lua_["dofile"] = sol::lua_nil;
    lua_["loadfile"] = sol::lua_nil;
    lua_["load"] = sol::lua_nil;

    // Bind C++ methods to Lua functions
    register_api();

    // ==========================================
    // SCRIPT LOADING AND VALIDATION
    // ==========================================

    // Layer 2: Protection - Safely load the user script
    auto result = lua_.safe_script_file(script_path, sol::script_pass_on_error);
    if (result.valid()) {
        valid_ = true;
        LOG_INFO("Flight script loaded successfully: " + script_path, "FlightComputer");
    } else {
        sol::error err = result;
        valid_ = false;
        LOG_ERROR("Failed to load flight script: " + std::string(err.what()), "FlightComputer");
    }
}

bool FlightComputerContext::is_valid() const {
    return valid_;
}

std::string FlightComputerContext::save_lua_globals() const {
    nlohmann::json out = nlohmann::json::object();
    if (!valid_) {
        return out.dump();
    }

    // Exclude Lua built-in / sandbox variables and API bindings
    static const std::vector<std::string> kExcludedKeys = {
        "_G", "_VERSION", "base", "math", "table", "string", "package", "coroutine", "os", "io", "debug", "utf8",
        "assert", "error", "pcall", "xpcall",
        "print", "select", "tonumber", "tostring", "type", "ipairs", "pairs", "next", "warn",
        "dofile", "loadfile", "load", "collectgarbage", "rawequal", "rawget", "rawlen", "rawset", "setmetatable", "getmetatable",
        "on_tick",
        "get_altitude", "get_vertical_velocity", "set_throttle", "print_log"
    };

    std::unordered_set<const void*> visited;
    for (const auto& pair : lua_.globals()) {
        if (!pair.first.is<std::string>()) {
            continue;
        }
        std::string key = pair.first.as<std::string>();
        if (std::find(kExcludedKeys.begin(), kExcludedKeys.end(), key) != kExcludedKeys.end()) {
            continue;
        }

        sol::type type = pair.second.get_type();
        if (type == sol::type::number) {
            out[key] = pair.second.as<double>();
        } else if (type == sol::type::boolean) {
            out[key] = pair.second.as<bool>();
        } else if (type == sol::type::string) {
            out[key] = pair.second.as<std::string>();
        } else if (type == sol::type::table) {
            auto table_json = sol_table_to_json(pair.second.as<sol::table>(), visited);
            if (!table_json.is_null()) {
                out[key] = table_json;
            }
        }
        // Functions, userdata, threads and nil are intentionally omitted by policy
    }

    return out.dump();
}

bool FlightComputerContext::restore_lua_globals(const std::string& json_str) {
    if (json_str.empty()) {
        return true;
    }
    try {
        const auto j = nlohmann::json::parse(json_str);
        if (!j.is_object()) {
            return false;
        }

        // Validate all types before modifying any state
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (!is_valid_json_lua_type(it.value())) {
                LOG_ERROR("Unsupported data type in Lua globals restore for key: " + it.key(), "FlightComputer");
                return false;
            }
        }

        for (auto it = j.begin(); it != j.end(); ++it) {
            lua_[it.key()] = json_to_sol(lua_, it.value());
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Error restoring Lua globals: " + std::string(e.what()), "FlightComputer");
        return false;
    }
}

void FlightComputerContext::register_api() {
    // ==========================================
    // C++ TO LUA API BRIDGE
    // ==========================================

    // READ FUNCTIONS: Allow Lua to read simulation telemetry
    lua_.set_function("get_altitude", [&]() -> float {
        return current_altitude_;
    });

    lua_.set_function("get_vertical_velocity", [&]() -> float {
        return current_vertical_velocity_;
    });

    // WRITE FUNCTIONS -> Allow Lua to send commands to the simulation
    lua_.set_function("set_throttle", [&](float value) {
        // Clamp throttle between 0% (0.0) and 100% (1.0)
        throttle_cmd_ = std::clamp(value, 0.0f, 1.0f);
    });

    // UTILITY FUNCTIONS -> Allow Lua to print to the simulation console
    lua_.set_function("print_log", [](std::string msg) {
        LOG_INFO(msg, "FlightComputer");
    });
}

void FlightComputerContext::update(const state_vector::GeneralState* in,
                                   flatbuffers::FlatBufferBuilder& fbb,
                                   float dt) {
    if (!in) return;

    // ==========================================
    // STEP 1: READ SIMULATION STATE (TELEMETRY)
    // ==========================================
    // Read current state from the physics engine into internal variables for Lua access.
    if (in->position()) {
        current_altitude_ = in->position()->z();
    } else {
        current_altitude_ = 0.0f;
    }

    if (in->velocity()) {
        current_vertical_velocity_ = in->velocity()->z();
    } else {
        current_vertical_velocity_ = 0.0f;
    }

    // Reset commands to zero before executing the script
    throttle_cmd_ = 0.0f;

    // ==========================================
    // STEP 2: EXECUTE USER SCRIPT
    // ==========================================
    if (valid_) {
        // Layer 2: Runtime protection - Catch script errors gracefully
        sol::protected_function on_tick = lua_["on_tick"];
        if (on_tick.valid()) {
            auto result = on_tick(dt);
            if (!result.valid()) {
                sol::error err = result;
                LOG_ERROR("Error executing on_tick: " + std::string(err.what()), "FlightComputer");
            }
        } else {
            LOG_ERROR("Function 'on_tick' not found in script", "FlightComputer");
        }
    }

    // ==========================================
    // STEP 3: REBUILD STATE (SEND COMMANDS)
    // ==========================================
    // FlatBuffers are immutable, so we must copy the entire state and insert our new commands.
    auto position = state_vector::Vec3(
        in->position() ? in->position()->x() : 0.0f,
        in->position() ? in->position()->y() : 0.0f,
        in->position() ? in->position()->z() : 0.0f);
    auto velocity = state_vector::Vec3(
        in->velocity() ? in->velocity()->x() : 0.0f,
        in->velocity() ? in->velocity()->y() : 0.0f,
        in->velocity() ? in->velocity()->z() : 0.0f);
    auto orientation = state_vector::Quaternion(
        in->orientation() ? in->orientation()->x() : 0.0f,
        in->orientation() ? in->orientation()->y() : 0.0f,
        in->orientation() ? in->orientation()->z() : 0.0f,
        in->orientation() ? in->orientation()->w() : 1.0f);
    auto angular_velocity = state_vector::Vec3(
        in->angular_velocity() ? in->angular_velocity()->x() : 0.0f,
        in->angular_velocity() ? in->angular_velocity()->y() : 0.0f,
        in->angular_velocity() ? in->angular_velocity()->z() : 0.0f);

    float total_mass = in->total_mass();
    auto cg_loc = state_vector::Vec3(
        in->cg_location() ? in->cg_location()->x() : 0.0f,
        in->cg_location() ? in->cg_location()->y() : 0.0f,
        in->cg_location() ? in->cg_location()->z() : 0.0f);

    state_vector::InertiaTensor inertia_tensor(
        in->inertia_tensor() ? in->inertia_tensor()->ixx() : 0.0f,
        in->inertia_tensor() ? in->inertia_tensor()->iyy() : 0.0f,
        in->inertia_tensor() ? in->inertia_tensor()->izz() : 0.0f,
        in->inertia_tensor() ? in->inertia_tensor()->ixy() : 0.0f,
        in->inertia_tensor() ? in->inertia_tensor()->ixz() : 0.0f,
        in->inertia_tensor() ? in->inertia_tensor()->iyz() : 0.0f);

    float mach_number = in->mach_number();
    float dynamic_pressure = in->dynamic_pressure();
    float angle_of_attack = in->angle_of_attack();
    float sideslip_angle = in->sideslip_angle();
    float atm_density = in->atm_density();
    float atm_pressure = in->atm_pressure();
    float atm_temperature = in->atm_temperature();

    flatbuffers::Offset<flatbuffers::Vector<float>> propellant_masses_fb;
    if (auto pm = in->propellant_masses()) {
        std::vector<float> pm_vec;
        for (auto v : *pm) pm_vec.push_back(v);
        propellant_masses_fb = fbb.CreateVector(pm_vec);
    }

    flatbuffers::Offset<flatbuffers::Vector<const state_vector::EngineCmd*>> engines_fb;
    if (auto engines = in->engines()) {
        std::vector<state_vector::EngineCmd> eng_vec;
        eng_vec.reserve(engines->size());
        for (size_t i = 0; i < engines->size(); ++i) {
            const state_vector::EngineCmd* ec = engines->Get(i);
            const state_vector::Vec3 tvc = ec->tvc_angles();
            // The script command overrides throttle on engine[0].
            // A PARALLEL_PHYSICS_CALCULATOR propulsion plugin is expected
            // to read this value and produce the corresponding thrust force.
            float throttle = (i == 0) ? throttle_cmd_ : ec->throttle();
            eng_vec.emplace_back(throttle, tvc);
        }
        engines_fb = fbb.CreateVectorOfStructs(eng_vec);
    }

    flatbuffers::Offset<flatbuffers::Vector<float>> surface_deflections_fb;
    if (auto sd = in->surface_deflections()) {
        std::vector<float> sd_vec;
        for (auto v : *sd) sd_vec.push_back(v);
        surface_deflections_fb = fbb.CreateVector(sd_vec);
    }

    auto wind_velocity = state_vector::Vec3(
        in->wind_velocity() ? in->wind_velocity()->x() : 0.0f,
        in->wind_velocity() ? in->wind_velocity()->y() : 0.0f,
        in->wind_velocity() ? in->wind_velocity()->z() : 0.0f);
    auto gravity = state_vector::Vec3(
        in->gravity() ? in->gravity()->x() : 0.0f,
        in->gravity() ? in->gravity()->y() : 0.0f,
        in->gravity() ? in->gravity()->z() : 0.0f);

    state_vector::GeneralStateBuilder gs_builder(fbb);
    gs_builder.add_sim_time(in->sim_time());
    gs_builder.add_dt(in->dt());
    gs_builder.add_position(&position);
    gs_builder.add_velocity(&velocity);
    gs_builder.add_orientation(&orientation);
    gs_builder.add_angular_velocity(&angular_velocity);
    gs_builder.add_total_mass(total_mass);
    gs_builder.add_cg_location(&cg_loc);
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
    fbb.Finish(general_state);
}

} // namespace Apogeo
