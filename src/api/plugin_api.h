#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <stdint.h>

/**
 * @file plugin_api.h
 * @brief Contrato C de interoperabilidad entre el host de MoLab y plugins dinámicos.
 */

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
     * @brief Estructura para representar un vector tridimensional.
     *
     * Se utiliza para fuerzas y torques intercambiados entre el host y el plugin.
     */
    typedef struct {
        float x;
        float y;
        float z;
    } PluginVector3;

    /**
     * @brief Datos intercambiados durante un tick de simulación.
     *
     * Incluye el buffer de estado, el paso de tiempo y punteros opcionales
     * para devolver fuerza y torque calculados por el plugin.
     */
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

        // Campos de compatibilidad (deprecated, usar output_force/output_torque)
        PluginVector3* force_out;
        PluginVector3* torque_out;

    } PluginTickData;

    typedef enum {
        PLUGIN_LOG_DEBUG = 0,
        PLUGIN_LOG_INFO = 1,
        PLUGIN_LOG_WARNING = 2,
        PLUGIN_LOG_ERROR = 3,
        PLUGIN_LOG_CRITICAL = 4
    } PluginLogLevel;

    typedef void (*PluginLogFn)(
        int32_t level,
        const char* component,
        const char* message,
        void* user_data
    );

    typedef struct {
        uint32_t api_version;
        PluginLogFn log;
        void* user_data;
    } PluginHostServices;

    // --- Funciones de la API---

    /**
     * @brief Crea una instancia del plugin y devuelve su handle.
     *
     * @return Handle válido de plugin o `NULL` si falla la creación.
     */
    PLUGIN_EXPORT PluginHandle plugin_create_instance();

    /**
     * @brief Configura una instancia del plugin con parámetros JSON.
     *
     * @param handle Handle del plugin a configurar.
     * @param json_params Cadena JSON con parámetros de configuración.
     * @return `0` si la configuración fue exitosa, valor negativo en caso de error.
     */
    PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params);

    /**
     * @brief Ejecuta un tick de simulación para una instancia del plugin.
     *
     * @param handle Handle del plugin a ejecutar.
     * @param data Datos de entrada/salida del tick.
     * @return `0` si la ejecución fue correcta, valor distinto de cero en caso de error.
     */
    PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle,
        PluginTickData* data
    );

    /**
     * @brief Destruye una instancia del plugin y libera sus recursos.
     *
     * @param handle Handle del plugin a destruir.
     */
    PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle);

    /**
     * @brief Registro de servicios del host (opcional).
     *
     * El host llama a esto si está presente; los plugins pueden ignorarlo si no lo necesitan.
     */
    PLUGIN_EXPORT void plugin_set_host_services(const PluginHostServices* services);


#ifdef __cplusplus
}
#endif

#endif // PLUGIN_API_H
