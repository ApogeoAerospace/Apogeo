#include "plugin_api.h"
#include "state_vector_generated.h"
#include <iostream>

// Define a simple plugin state as an example
struct PluginState {
    int tick_count = 0;
};

// --- API function implementations ---

PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    std::cout << "[Plugin] Creating instance..." << std::endl;
    // Handle is a pointer to PluginState
    PluginState* state = new PluginState();
    return reinterpret_cast<PluginHandle>(state);
}

PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    std::cout << "[Plugin] Destroying instance..." << std::endl;
    PluginState* state = reinterpret_cast<PluginState*>(handle);
    // Release plugin resources here
    delete state;
}

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    // 1. Access plugin internal state
    PluginState* state = reinterpret_cast<PluginState*>(handle);
    state->tick_count++;
    std::cout << "[Plugin] Tick #" << state->tick_count << " received." << std::endl;

    // 2. Get vehicle state from buffer through the 'data' struct
    auto mutable_state = state_vector::GetMutableGeneralState(data->state_buffer);

    float current_x = mutable_state->position()->x();
    std::cout << "[Plugin] Data received -> Position X: " << current_x << std::endl;

    mutable_state->mutable_position()->mutate_x(current_x + 1.0f);
    std::cout << "[Plugin] Position X changed to: " << mutable_state->position()->x() << std::endl;

    return 0;
}
