#include "FlightComputerContext.h"
#include "Logger.h"
#include <algorithm>

/**
 * @file FlightComputerContext.cpp
 * @brief Implementation of the embedded Lua flight computer context.
 */

namespace MoLab {

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

void FlightComputerContext::register_api() {
    // ==========================================
    // C++ TO LUA API BRIDGE
    // ==========================================

    // READ FUNCTIONS: Allow Lua to read simulation telemetry
    lua_.set_function("get_altitude", [&]() -> float {
        return current_altitude_;
    });

    lua_.set_function("get_velocity", [&]() -> float {
        return current_velocity_;
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
        current_velocity_ = in->velocity()->x();
    } else {
        current_velocity_ = 0.0f;
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

} // namespace MoLab
