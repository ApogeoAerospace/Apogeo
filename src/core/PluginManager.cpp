#include "PluginManager.h"
#include <iostream>
#include <vector>
#include <cstdint>
#include <chrono>
#include <thread>
#include <numeric>

#if defined(_WIN32)
#include <windows.h>
#endif

PluginManager::~PluginManager() {
    shutdown();
}

bool PluginManager::load_plugin(const std::string& path, PluginType type) {
    LoadedPlugin plugin;
    plugin.type = type; // Asigna el tipo de plugin

#if defined(_WIN32)
    plugin.lib_handle = LoadLibrary(path.c_str());
    if (!plugin.lib_handle) {
        std::cerr << "[Core] ERROR: No se pudo cargar el plugin " << path << std::endl;
        return false;
    }
    plugin.create_func = (decltype(plugin.create_func))GetProcAddress(plugin.lib_handle, "plugin_create_instance");
    plugin.tick_func = (decltype(plugin.tick_func))GetProcAddress(plugin.lib_handle, "plugin_tick");
    plugin.destroy_func = (decltype(plugin.destroy_func))GetProcAddress(plugin.lib_handle, "plugin_destroy_instance");
#else // POSIX
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

    // Clasifica el plugin en el vector correcto
    switch (type) {
    case PluginType::SEQUENTIAL_STATE_MODIFIER:
        sequential_plugins_.push_back(plugin);
        break;
    case PluginType::PARALLEL_PHYSICS_CALCULATOR:
        parallel_plugins_.push_back(plugin);
        break;
    }

    std::cout << "[Core] Plugin cargado e instanciado: " << path << std::endl;
    return true;
}

void PluginManager::run_simulation_cycle(std::vector<uint8_t>& state_buffer) {
    PluginTickData tick_data = {};
    tick_data.state_buffer = state_buffer.data();
    tick_data.buffer_size = state_buffer.size();

    // --- FASE 1: Plugins Secuenciales ---
    tick_data.force_out = nullptr;
    tick_data.torque_out = nullptr;
    for (const auto& plugin : sequential_plugins_) {
        plugin.tick_func(plugin.instance, &tick_data);
    }

    // --- FASE 2: Plugins Paralelos ---
    std::vector<PluginVector3> forces(parallel_plugins_.size());
    std::vector<PluginVector3> torques(parallel_plugins_.size());
    std::vector<std::thread> threads;
    threads.reserve(parallel_plugins_.size());

    for (size_t i = 0; i < parallel_plugins_.size(); ++i) {
        PluginTickData parallel_tick_data = {};
        parallel_tick_data.state_buffer = tick_data.state_buffer;
        parallel_tick_data.buffer_size = tick_data.buffer_size;
        parallel_tick_data.force_out = &forces[i];
        parallel_tick_data.torque_out = &torques[i];
        threads.emplace_back(parallel_plugins_[i].tick_func, parallel_plugins_[i].instance, &parallel_tick_data);
    }
    for (auto& t : threads) {
        t.join();
    }

    // --- FASE 3: Integración ---

    PluginVector3 total_force = { 0.0f, 0.0f, 0.0f };
    for (const auto& f : forces) {
        total_force.x += f.x;
        total_force.y += f.y;
        total_force.z += f.z;
    }

    PluginVector3 total_torque = { 0.0f, 0.0f, 0.0f };
    for (const auto& t : torques) {
        total_torque.x += t.x;
        total_torque.y += t.y;
        total_torque.z += t.z;
    }

    this->_apply_total_force_and_torque(total_force, total_torque, tick_data);
}

void PluginManager::shutdown() {
    auto shutdown_plugin_list = [&](std::vector<LoadedPlugin>& list) {
        for (auto& plugin : list) {
            if (plugin.instance) plugin.destroy_func(plugin.instance);
#if defined(_WIN32)
            if (plugin.lib_handle) FreeLibrary(plugin.lib_handle);
#else
            if (plugin.lib_handle) dlclose(plugin.lib_handle);
#endif
        }
        list.clear();
        };

    shutdown_plugin_list(sequential_plugins_);
    shutdown_plugin_list(parallel_plugins_);

    std::cout << "[Core] Todos los plugins han sido liberados." << std::endl;
}
