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

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    // 1. Acceder al estado interno del plugin (sin cambios)
    PluginState* state = reinterpret_cast<PluginState*>(handle);
    state->tick_count++;
    std::cout << "[Plugin] Tick #" << state->tick_count << " recibido." << std::endl;

    // 2. Obtener el estado del vehículo desde el buffer a través de la estructura 'data'
    // La única línea que cambia: 'buffer' ahora es 'data->state_buffer'
    auto mutable_state = state_vector::GetMutableVehicleState(data->state_buffer);

    // 3. El resto de la lógica de mutación permanece igual
    float current_x = mutable_state->position()->x();
    std::cout << "[Plugin] Datos recibidos -> Posición X: " << current_x << std::endl;

    mutable_state->mutable_position()->mutate_x(current_x + 1.0f);
    std::cout << "[Plugin] Posición X modificada a: " << mutable_state->position()->x() << std::endl;

    // 4. Retornar el código de estado (0 para éxito)
    return 0;
}
