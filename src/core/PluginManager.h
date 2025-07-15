#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "plugin_api.h"
#include <string>
#include <vector>

// Definición de un handle para manejar instancias de plugins
// min y max de windows.h causa problemas con std::min y std::max
#if defined(_WIN32)
    #define NOMINMAX
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

/*
* Estructura que representa un plugin cargado
*/
struct LoadedPlugin {

    // Handle de la instancia del plugin
    PluginHandle instance = nullptr;

    // Funciones de la API del plugin
    // Función para crear una instancia del plugin
    PluginHandle(*create_func)() = nullptr;
    // Función que se ejecuta en cada tick del plugin
    uint32_t(*tick_func)(PluginHandle, uint8_t*, uint32_t) = nullptr;
    // Función para destruir la instancia del plugin y liberar recursos
    void (*destroy_func)(PluginHandle) = nullptr;

    #if defined(_WIN32)
        HMODULE lib_handle = nullptr;
    #else
        void* lib_handle = nullptr;
    #endif
};

/*
* PluginManager es la clase encargada de gestionar los plugins.
* Permite cargar plugins, ejecutar sus ticks y liberar recursos.
*/
class PluginManager {
public:
    ~PluginManager();

    /*
    * Carga la librería dinámica del plugin y obtiene las funciones de la API.
    */
    bool load_plugin(const std::string& path);

    /*
    * Corre todos los plugins cargados, pasando el buffer de estado.
    */
    void run_all_plugins(uint8_t* state_buffer, uint32_t buffer_size);

    /*
    * Libera todos los recursos de los plugins cargados.
    */
    void shutdown();

private:
    std::vector<LoadedPlugin> plugins_;
};

#endif
