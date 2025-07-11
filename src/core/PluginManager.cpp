#include "PluginManager.h"
#include "state_vector_generated.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <chrono>

#if defined(_WIN32)
#include <windows.h>
#endif

PluginManager::~PluginManager() {
    shutdown();
}

bool PluginManager::load_plugin(const std::string& path) {
    LoadedPlugin plugin;

#if defined(_WIN32)
    plugin.lib_handle = LoadLibrary(path.c_str());
    if (!plugin.lib_handle) {
        std::cerr << "[Core] ERROR: No se pudo cargar el plugin " << path << std::endl;
        return false;
    }

    plugin.create_func = (decltype(plugin.create_func))GetProcAddress(plugin.lib_handle, "plugin_create_instance");
    plugin.tick_func = (decltype(plugin.tick_func))GetProcAddress(plugin.lib_handle, "plugin_tick");
    plugin.destroy_func = (decltype(plugin.destroy_func))GetProcAddress(plugin.lib_handle, "plugin_destroy_instance");
#else

    plugin.lib_handle = dlopen(path.c_str(), RTLD_LAZY);
    if (!plugin.lib_handle) {
        std::cerr << "[Core] ERROR: No se pudo cargar el plugin " << path << ": " << dlerror() << std::endl;
        return false;
    }

    plugin.create_func = (decltype(plugin.create_func))dlsym(plugin.lib_handle, "plugin_create_instance");
    plugin.tick_func = (decltype(plugin.tick_func))dlsym(plugin.lib_handle, "plugin_tick");
    plugin.destroy_func = (decltype(plugin.destroy_func))dlsym(plugin.lib_handle, "plugin_destroy_instance");

#endif

    if (!plugin.create_func || !plugin.tick_func || !plugin.destroy_func) {
        std::cerr << "[Core] ERROR: No se pudieron encontrar las funciones de la API en " << path << std::endl;
#if defined(_WIN32)
        FreeLibrary(plugin.lib_handle);
#else
        dlclose(plugin.lib_handle);
#endif
        return false;
    }

    plugin.instance = plugin.create_func();
    plugins_.push_back(plugin);
    std::cout << "[Core] Plugin cargado e instanciado: " << path << std::endl;
    return true;
}

void PluginManager::run_all_plugins(uint8_t* state_buffer, uint32_t buffer_size) {
    if (plugins_.empty()) {
        return;
    }

    for (const auto& plugin : plugins_) {
        auto start = std::chrono::high_resolution_clock::now();

        plugin.tick_func(plugin.instance, state_buffer, buffer_size);

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> elapsed_micros = end - start;
        std::cout << "[Metrics] Latencia del tick del plugin: "
            << elapsed_micros.count() << " microsegundos." << std::endl;
    }
}

void PluginManager::shutdown() {
    if (plugins_.empty()) {
        return;
    }

    for (auto& plugin : plugins_) {
        if (plugin.instance) {
            plugin.destroy_func(plugin.instance);
        }
#if defined(_WIN32)
        if (plugin.lib_handle) {
            FreeLibrary(plugin.lib_handle);
        }
#else
        if (plugin.lib_handle) {
            dlclose(plugin.lib_handle);
        }
#endif
    }
    plugins_.clear();
    std::cout << "[Core] Todos los plugins han sido liberados." << std::endl;
}
