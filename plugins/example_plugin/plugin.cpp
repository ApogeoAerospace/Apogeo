#include "plugin_api.h"
#include "state_vector_generated.h"
#include <iostream>

// Como ejemplo se define un estado simple del plugin
struct PluginState {
    int tick_count = 0;
};

// --- Implementación de las funciones de la API ---

PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    std::cout << "[Plugin] Creando instancia..." << std::endl;
    // El handle es un puntero a PluginState
    PluginState* state = new PluginState();
    return reinterpret_cast<PluginHandle>(state);
}

PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    std::cout << "[Plugin] Destruyendo instancia..." << std::endl;
    PluginState* state = reinterpret_cast<PluginState*>(handle);
    // Aquí se liberan los recursos del plugin
    delete state;
}

PLUGIN_EXPORT uint32_t plugin_tick(PluginHandle handle,
    uint8_t* buffer, uint32_t size) {

    PluginState* state = reinterpret_cast<PluginState*>(handle);
    // Al avanzar el tick, se incrementa el contador del estado del plugin
    state->tick_count++;
    std::cout << "[Plugin] Tick #" << state->tick_count << " recibido." << std::endl;

    // Consigue el estado del vehículo desde el buffer. Además, es modificable, lo que permite cambiar un item sin crear un nuevo buffer.
    auto mutable_state = state_vector::GetMutableVehicleState(buffer);

    float current_x = mutable_state->position()->x();
    std::cout << "[Plugin] Datos recibidos -> Posición X: " << current_x << std::endl;

    mutable_state->mutable_position()->mutate_x(current_x + 1.0f);
    std::cout << "[Plugin] Posición X modificada a: " << mutable_state->position()->x() << std::endl;

    return 0;
}
