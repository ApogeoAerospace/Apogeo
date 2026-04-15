#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#include <stdint.h>

/**
 * @file plugin_api.h
 * @brief C interoperability contract between the MoLab host and dynamic plugins.
 */

// Definition of PLUGIN_EXPORT macro for plugin function exports
#if defined(_WIN32) // WINDOWS

#define PLUGIN_EXPORT __declspec(dllexport)

#else // POSIX

#define PLUGIN_EXPORT __attribute__((visibility("default")))

#endif // FIN IF

// Opaque pointer used to manage plugin instances
typedef struct PluginInstance* PluginHandle;

// Prevent C++ name mangling
#ifdef __cplusplus
extern "C" {
#endif

    // --- Data Structures ---

    /**
     * @brief Structure representing a 3D vector.
     *
     * Used for forces and torques exchanged between host and plugin.
     */
    typedef struct {
        float x;
        float y;
        float z;
    } PluginVector3;

    /**
     * @brief Data exchanged during a simulation tick.
     *
     * Includes the state buffer, time step, and optional output pointers
     * for plugin-calculated force and torque.
     */
    typedef struct {
        // --- INPUT/OUTPUT ---
        // Pointer to the central state buffer. Plugins may modify it.
        uint8_t* state_buffer;
        uint32_t buffer_size;

        // --- INPUT ---
        // Delta time for this simulation tick
        double delta_time;

        // --- OUTPUT ---
        // Host provides valid pointers if it expects dynamic outputs from plugin.
        // If NULL, the plugin must ignore them.
        PluginVector3* output_force;
        PluginVector3* output_torque;

        // Compatibility fields (deprecated, use output_force/output_torque)
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

    // --- API Functions ---

    /**
     * @brief Creates a plugin instance and returns its handle.
     *
     * @return Valid plugin handle or `NULL` if creation fails.
     */
    PLUGIN_EXPORT PluginHandle plugin_create_instance();

    /**
     * @brief Configures a plugin instance using JSON parameters.
     *
     * @param handle Plugin handle to configure.
     * @param json_params JSON string with configuration parameters.
     * @return `0` on success, negative value on error.
     */
    PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params);

    /**
     * @brief Executes a simulation tick for a plugin instance.
     *
     * @param handle Plugin handle to execute.
     * @param data Tick input/output data.
     * @return `0` on success, non-zero value on error.
     */
    PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle,
        PluginTickData* data
    );

    /**
     * @brief Destroys a plugin instance and releases resources.
     *
     * @param handle Plugin handle to destroy.
     */
    PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle);

    /**
     * @brief Host services registration (optional).
     *
     * Host calls this when available; plugins may ignore it if not needed.
     */
    PLUGIN_EXPORT void plugin_set_host_services(const PluginHostServices* services);


#ifdef __cplusplus
}
#endif

#endif // PLUGIN_API_H
