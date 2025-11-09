#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "plugin_api.h"
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <future>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// Define el rol de cada plugin para la orquestación.
enum class PluginType {
    SEQUENTIAL_STATE_MODIFIER = 0,
    PARALLEL_PHYSICS_CALCULATOR = 1
};

// Forward declarations
namespace MoLab {
    class PhysicsIntegrator;
}

/**
 * @brief Estructura que representa un plugin cargado con información extendida.
 */
struct LoadedPlugin {
    PluginHandle handle = nullptr;
    PluginType type = PluginType::SEQUENTIAL_STATE_MODIFIER;
    std::string path;
    std::string name;
    bool enabled = true;

    // Métricas de rendimiento
    std::atomic<uint64_t> execution_count{0};
    std::atomic<double> total_execution_time{0.0};
    std::atomic<double> last_execution_time{0.0};

    // Funciones de la API del plugin
    PluginHandle(*create_func)() = nullptr;
    int32_t(*configure_func)(PluginHandle, const char*) = nullptr;
    int32_t(*tick_func)(PluginHandle, PluginTickData*) = nullptr;
    void(*destroy_func)(PluginHandle) = nullptr;

#if defined(_WIN32)
    HMODULE lib_handle = nullptr;
#else
    void* lib_handle = nullptr;
#endif

    // Constructor por defecto
    LoadedPlugin() = default;

    // Eliminar constructor de copia y operador de asignación
    LoadedPlugin(const LoadedPlugin&) = delete;
    LoadedPlugin& operator=(const LoadedPlugin&) = delete;

    // Permitir constructor de movimiento y operador de asignación de movimiento
    LoadedPlugin(LoadedPlugin&& other) noexcept
        : handle(other.handle),
          type(other.type),
          path(std::move(other.path)),
          name(std::move(other.name)),
          enabled(other.enabled),
          execution_count(other.execution_count.load()),
          total_execution_time(other.total_execution_time.load()),
          last_execution_time(other.last_execution_time.load()),
          create_func(other.create_func),
          configure_func(other.configure_func),
          tick_func(other.tick_func),
          destroy_func(other.destroy_func),
          lib_handle(other.lib_handle) {

        // Reset other object
        other.handle = nullptr;
        other.create_func = nullptr;
        other.configure_func = nullptr;
        other.tick_func = nullptr;
        other.destroy_func = nullptr;
        other.lib_handle = nullptr;
    }

    LoadedPlugin& operator=(LoadedPlugin&& other) noexcept {
        if (this != &other) {
            handle = other.handle;
            type = other.type;
            path = std::move(other.path);
            name = std::move(other.name);
            enabled = other.enabled;
            execution_count.store(other.execution_count.load());
            total_execution_time.store(other.total_execution_time.load());
            last_execution_time.store(other.last_execution_time.load());
            create_func = other.create_func;
            configure_func = other.configure_func;
            tick_func = other.tick_func;
            destroy_func = other.destroy_func;
            lib_handle = other.lib_handle;

            // Reset other object
            other.handle = nullptr;
            other.create_func = nullptr;
            other.configure_func = nullptr;
            other.tick_func = nullptr;
            other.destroy_func = nullptr;
            other.lib_handle = nullptr;
        }
        return *this;
    }
};

/**
 * @brief PluginManager gestiona el ciclo de vida y la ejecución de los plugins con thread safety.
 */
class PluginManager {
public:
    PluginManager();
    ~PluginManager();

    // Prevenir copia y asignación
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /**
     * @brief Carga un plugin y lo clasifica según su tipo.
     */
    bool load_plugin(const std::string& path, PluginType type);

    /**
     * @brief Carga plugins desde configuración
     */
    bool load_plugins_from_config();

    /**
     * @brief Descarga un plugin específico
     */
    bool unload_plugin(const std::string& path);

    /**
     * @brief Habilita o deshabilita un plugin
     */
    bool set_plugin_enabled(const std::string& path, bool enabled);

    /**
     * @brief Ejecuta un ciclo de simulación con todos los plugins cargados.
     */
    void run_simulation_cycle(std::vector<uint8_t>& state_buffer);

    /**
     * @brief Ejecuta un ciclo de simulación mejorado con integración física
     */
    void run_simulation_cycle_improved(std::vector<uint8_t>& state_buffer, double delta_time);

    /**
     * @brief Libera todos los recursos y descarga todos los plugins.
     */
    void shutdown();

    // Métodos de información y estadísticas
    size_t get_plugin_count() const;
    std::vector<std::string> get_loaded_plugin_names() const;
    bool is_plugin_loaded(const std::string& path) const;

    // Métricas de rendimiento
    struct PluginMetrics {
        std::string name;
        uint64_t execution_count;
        double total_execution_time;
        double average_execution_time;
        double last_execution_time;
    };

    std::vector<PluginMetrics> get_plugin_metrics() const;
    void reset_plugin_metrics();

private:
    // Contenedores de plugins thread-safe
    std::vector<LoadedPlugin> loaded_plugins_;
    mutable std::mutex plugins_mutex_;

    // Métricas globales
    std::atomic<uint64_t> total_cycles_{0};
    std::atomic<double> total_cycle_time_{0.0};

    // Métodos privados
    void execute_sequential_plugins(std::vector<uint8_t>& state_buffer);
    void execute_parallel_plugins(std::vector<uint8_t>& state_buffer);
    void apply_physics_integration(std::vector<uint8_t>& state_buffer, double delta_time);

    // Utilidades
    std::vector<LoadedPlugin*> get_plugins_by_type(PluginType type);
    bool validate_plugin_api(const LoadedPlugin& plugin) const;
    void cleanup_plugin(LoadedPlugin& plugin);

    // Integrador físico
    std::unique_ptr<MoLab::PhysicsIntegrator> physics_integrator_;

    // Almacenamiento de fuerzas para integración física
    PluginVector3 accumulated_force_{0.0f, 0.0f, 0.0f};
    PluginVector3 accumulated_torque_{0.0f, 0.0f, 0.0f};
    mutable std::mutex force_mutex_;
};

#endif
