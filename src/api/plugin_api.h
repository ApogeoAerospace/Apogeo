#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <stdint.h>

#if defined(_WIN32)
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

typedef struct PluginInstance* PluginHandle;

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * Crea una instancia del plugin y devuelve un handle a ella.
     */
    PLUGIN_EXPORT PluginHandle plugin_create_instance();

    /**
     * Ejecuta un tick de la simulación para una instancia del plugin.
     */
    PLUGIN_EXPORT uint32_t plugin_tick(PluginHandle handle,
        uint8_t* buffer,
        uint32_t size
    );

    /**
     * Destruye una instancia del plugin y libera sus recursos.
     */
    PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle);


#ifdef __cplusplus
}
#endif

#endif // PLUGIN_API_H
