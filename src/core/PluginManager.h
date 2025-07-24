#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "plugin_api.h"
#include <string>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Define el rol de cada plugin para la orquestación.
enum class PluginType {
    SEQUENTIAL_STATE_MODIFIER,
    PARALLEL_PHYSICS_CALCULATOR
};

/**
 * @brief Estructura que representa un plugin cargado.
 */
struct LoadedPlugin {
    PluginHandle instance = nullptr;
    PluginType type;

    // --- Punteros a funciones de la API actualizada ---
    PluginHandle(*create_func)() = nullptr;
    // La firma de tick_func ahora usa PluginTickData
    int32_t(*tick_func)(PluginHandle, PluginTickData*) = nullptr;
    void(*destroy_func)(PluginHandle) = nullptr;

#if defined(_WIN32)
    HMODULE lib_handle = nullptr;
#else
    void* lib_handle = nullptr;
#endif
};

/**
 * @brief PluginManager gestiona el ciclo de vida y la ejecución de los plugins.
 */
class PluginManager {
public:
    ~PluginManager();

    /**
     * @brief Carga un plugin y lo clasifica según su tipo.
     */
    bool load_plugin(const std::string& path, PluginType type);

    /**
     * @brief Orquesta y ejecuta un ciclo completo de la simulación.
     */
    void run_simulation_cycle(std::vector<uint8_t>& state_buffer);

    /**
     * @brief Libera todos los plugins y recursos.
     */
    void shutdown();

private:
    std::vector<LoadedPlugin> sequential_plugins_;
    std::vector<LoadedPlugin> parallel_plugins_;

    void _apply_total_force_and_torque(const PluginVector3& total_force, const PluginVector3& total_torque, PluginTickData& tick_data) {
        return;
    }
};

#endif
