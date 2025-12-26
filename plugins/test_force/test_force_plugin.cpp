/*
 * Test Force Plugin - Plugin de Prueba para Generación de Fuerzas
 *
 * Este plugin genera fuerzas constantes para probar el sistema de integración
 * física de MoLab. Es un plugin de tipo PARALLEL_PHYSICS_CALCULATOR.
 */

#include "../../src/api/plugin_api.h"
#include <cmath>

// Estructura interna del plugin
struct TestForcePluginInstance {
    double upward_force;     // Fuerza hacia arriba (N)
    double lateral_force;    // Fuerza lateral (N)
    double oscillation_freq; // Frecuencia de oscilación (Hz)
    double time_offset;      // Offset de tiempo para variación
    bool initialized;

    // Configuración
    bool enable_upward_force;
    bool enable_lateral_force;
    bool enable_oscillation;
};

extern "C" {

PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    TestForcePluginInstance* instance = new TestForcePluginInstance();

    // Configuración inicial
    instance->upward_force = 5000.0;      // 5000N hacia arriba
    instance->lateral_force = 1000.0;     // 1000N lateral
    instance->oscillation_freq = 0.5;     // 0.5 Hz
    instance->time_offset = 0.0;
    instance->initialized = true;

    // Flags de control
    instance->enable_upward_force = true;
    instance->enable_lateral_force = true;
    instance->enable_oscillation = false;  // Deshabilitado por defecto

    return reinterpret_cast<PluginHandle>(instance);
}

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data) {
        return -1; // Error: parámetros inválidos
    }

    TestForcePluginInstance* instance = reinterpret_cast<TestForcePluginInstance*>(handle);
    if (!instance->initialized) {
        return -2; // Error: instancia no inicializada
    }

    // Para esta versión simple, no parseamos el estado FlatBuffer
    // En su lugar, usamos un tiempo simulado simple
    static double simulated_time = 0.0;
    simulated_time += 0.1; // Incrementar tiempo simulado

    // Inicializar fuerzas y torques
    PluginVector3 force = {0.0f, 0.0f, 0.0f};
    PluginVector3 torque = {0.0f, 0.0f, 0.0f};

    // *** FUERZA HACIA ARRIBA (Anti-gravedad) ***
    if (instance->enable_upward_force) {
        double upward = instance->upward_force;

        // Añadir oscilación si está habilitada
        if (instance->enable_oscillation) {
            double oscillation = std::sin(2.0 * M_PI * instance->oscillation_freq * simulated_time);
            upward *= (1.0 + 0.3 * oscillation); // ±30% de variación
        }

        force.z += static_cast<float>(upward);
    }

    // *** FUERZA LATERAL (Movimiento horizontal) ***
    if (instance->enable_lateral_force) {
        double lateral = instance->lateral_force;

        // Añadir oscilación si está habilitada
        if (instance->enable_oscillation) {
            double oscillation = std::cos(2.0 * M_PI * instance->oscillation_freq * simulated_time);
            lateral *= (1.0 + 0.5 * oscillation); // ±50% de variación
        }

        force.x += static_cast<float>(lateral);
    }

    // *** TORQUE DE ROTACIÓN (Opcional) ***
    // Pequeño torque para crear rotación visible
    torque.y = 50.0f * std::sin(2.0 * M_PI * 0.1 * simulated_time); // Rotación lenta

    // Output forces if requested
    if (data->output_force) {
        *data->output_force = force;
    }

    if (data->output_torque) {
        *data->output_torque = torque;
    }

    return 0; // Éxito
}

PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (handle) {
        TestForcePluginInstance* instance = reinterpret_cast<TestForcePluginInstance*>(handle);
        delete instance;
    }
}

} // extern "C"

/*
 * CONFIGURACIÓN PARA COMPILAR Y USAR ESTE PLUGIN:
 *
 * 1. Compilar como shared library:
 *    cd /Documents/MoLab/build
 *    g++ -shared -fPIC -std=c++17 -I../src -I. \
 *        -o plugins/libtest_force.dylib \
 *        ../plugins/test_force/test_force_plugin.cpp
 *
 * 2. Añadir a la configuración JSON (force_test_config.json):
 * {
 *   "plugins": [
 *     {
 *       "name": "test_force",
 *       "type": 1,
 *       "library_path": "plugins/libtest_force",
 *       "enabled": true
 *     }
 *   ]
 * }
 *
 * 3. Resultado esperado con masa de 1000kg:
 *    - Gravedad: -9810N hacia abajo
 *    - Plugin: +5000N hacia arriba
 *    - Neto: -4810N hacia abajo (caída más lenta)
 *    - Fuerza lateral: +1000N (movimiento horizontal)
 *    - Amortiguamiento: reduce velocidad gradualmente
 *    - Rotación: pequeño torque para rotación visible
 *
 * 4. Para probar:
 *    ./bin/simulator --config ../data/config/force_test_config.json --ticks 50
 */
