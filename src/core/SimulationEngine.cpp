#include "SimulationEngine.h"
#include "PluginManager.h"
#include "InitialStateGenerator.h"
#include "flatbuffers/flatbuffers.h"

SimulationEngine::SimulationEngine()
    : plugin_manager_(std::make_unique<PluginManager>())
{
}

SimulationEngine::~SimulationEngine() {
}

bool SimulationEngine::initialize(const std::string& state_filepath) {
    flatbuffers::FlatBufferBuilder builder;
    if (!InitialStateGenerator::create_state_from_json(builder, state_filepath)) {
        return false;
    }

    const uint8_t* buf = builder.GetBufferPointer();
    const uint32_t size = builder.GetSize();
    // Asignamos el buffer de estado actual con los datos generados
    current_state_buffer_.assign(buf, buf + size);
    return true;
}

void SimulationEngine::load_plugin(const std::string& path) {
    plugin_manager_->load_plugin(path);
}

void SimulationEngine::run_tick() {
    // Verifica si hay plugins cargados
    if (current_state_buffer_.empty()) {
        return;
    }

    // Ejecuta todos los plugins con el estado actual
    plugin_manager_->run_all_plugins(
        current_state_buffer_.data(),
        current_state_buffer_.size()
    );
}

void SimulationEngine::shutdown() {
    plugin_manager_->shutdown();
}
