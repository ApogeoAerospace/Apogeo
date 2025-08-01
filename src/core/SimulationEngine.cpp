#include <iostream>
#include "SimulationEngine.h"
#include "PluginManager.h"
#include "InitialStateLoader.h"
#include "flatbuffers/flatbuffers.h"

SimulationEngine::SimulationEngine()
    : plugin_manager_(std::make_unique<PluginManager>())
{
}

SimulationEngine::~SimulationEngine() {
}

bool SimulationEngine::initialize(const std::string& state_filepath) {
    flatbuffers::FlatBufferBuilder builder;
    if (!InitialStateLoader::create_state_from_json(builder, state_filepath)) {
        return false;
    }

    const uint8_t* buf = builder.GetBufferPointer();
    const uint32_t size = builder.GetSize();
    // Asignamos el buffer de estado actual con los datos generados
    current_state_buffer_.assign(buf, buf + size);
    return true;
}

void SimulationEngine::load_plugin(const std::string& name, int plugin_type) {
    // Traducimos el tipo de plugin a la enumeración adecuada
    PluginType type;
    switch (plugin_type) {
        case 0:
            type = PluginType::SEQUENTIAL_STATE_MODIFIER;
            break;
        case 1:
            type = PluginType::PARALLEL_PHYSICS_CALCULATOR;
            break;
        default:
            std::cerr << "[Core] ERROR: Tipo de plugin desconocido: " << plugin_type << std::endl;
            return;
    }
#if defined(_WIN32)
    std::string path = name + ".dll";
#elif defined(__APPLE__)
    std::string rel_path = "../lib/lib" + name + ".dylib";
    char abs_path[PATH_MAX];
    if (realpath(rel_path.c_str(), abs_path) != nullptr) {
        std::string path = abs_path;
        plugin_manager_->load_plugin(path, type);
    } else {
        std::cerr << "[Core] ERROR: No se encontró el plugin en: " << rel_path << std::endl;
    }
    return;
#else
    std::string rel_path = "../lib/lib" + name + ".so";
    char abs_path[PATH_MAX];
    if (realpath(rel_path.c_str(), abs_path) != nullptr) {
        std::string path = abs_path;
        plugin_manager_->load_plugin(path, type);
    } else {
        std::cerr << "[Core] ERROR: No se encontró el plugin en: " << rel_path << std::endl;
    }
    return;
#endif
}

void SimulationEngine::run_tick() {
    // Verifica si hay plugins cargados
    if (current_state_buffer_.empty() || !plugin_manager_) {
        return;
    }

    // Le pasamos el buffer de estado para que pueda ser modificado.
    plugin_manager_->run_simulation_cycle(current_state_buffer_);
}

void SimulationEngine::shutdown() {
    plugin_manager_->shutdown();
}
