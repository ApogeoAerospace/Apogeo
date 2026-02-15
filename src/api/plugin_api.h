#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <stdint.h>

// Definición de la macro PLUGIN_EXPORT para exportar funciones del plugin
#if defined(_WIN32) // WINDOWS

#define PLUGIN_EXPORT __declspec(dllexport)

#else // POSIX

#define PLUGIN_EXPORT __attribute__((visibility("default")))

#endif // FIN IF

// Pointer opaco para manejar instancias del plugin
typedef struct PluginInstance* PluginHandle;

// Para evitar name mangling en C++
#ifdef __cplusplus
extern "C" {
#endif

    // --- Estructuras de Datos ---

    /**
     * @brief Estructura para un vector de 3 componentes (fuerza, torque).
     */
    typedef struct {
        float x;
        float y;
        float z;
    } PluginVector3;

    typedef struct {
        // --- ENTRADA/SALIDA ---
        // Puntero al buffer de estado central. Los plugins pueden modificarlo.
        uint8_t* state_buffer;
        uint32_t buffer_size;

        // --- ENTRADA ---
        // Tiempo delta para este tick de simulación
        double delta_time;

        // --- SALIDA ---
        // El host provee punteros válidos si espera que el plugin calcule dinámicas.
        // Si son NULL, el plugin los debe ignorar.
        PluginVector3* output_force;
        PluginVector3* output_torque;

        // Masa actual del vehículo (kg). El plugin de propulsión la actualiza
        // para reflejar el consumo de combustible. Si es NULL, se ignora.
        // Si el valor es <= 0 después del tick, el host usa la masa por defecto.
        double* output_mass;

        // Campos de compatibilidad (deprecated, usar output_force/output_torque)
        PluginVector3* force_out;
        PluginVector3* torque_out;

    } PluginTickData;

    // --- Funciones de la API---

    /**
     * Crea una instancia del plugin y devuelve un handle a ella.
     */
    PLUGIN_EXPORT PluginHandle plugin_create_instance();

    /**
     * Configura una instancia del plugin con parámetros JSON.
     * @param handle Handle del plugin a configurar
     * @param json_params Cadena JSON con los parámetros de configuración
     * @return 0 si la configuración fue exitosa, código de error negativo en caso contrario
     */
    PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params);

    /**
     * Ejecuta un tick de la simulación para una instancia del plugin.
     */
    PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle,
        PluginTickData* data
    );

    /**
     * Destruye una instancia del plugin y libera sus recursos.
     */
    PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle);


#ifdef __cplusplus
}
#endif

#endif // PLUGIN_API_H
