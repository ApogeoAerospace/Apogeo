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

struct LoadedPlugin {
    PluginHandle instance = nullptr;
    PluginHandle(*create_func)() = nullptr;
    uint32_t(*tick_func)(PluginHandle, uint8_t*, uint32_t) = nullptr;
    void (*destroy_func)(PluginHandle) = nullptr;
    #if defined(_WIN32)
        HMODULE lib_handle = nullptr;
    #else
        void* lib_handle = nullptr;
    #endif
};

class PluginManager {
public:
    ~PluginManager();

    bool load_plugin(const std::string& path);

    void run_all_plugins(uint8_t* state_buffer, uint32_t buffer_size);

    void shutdown();

private:
    std::vector<LoadedPlugin> plugins_;
};

#endif
