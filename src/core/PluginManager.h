#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "plugin_api.h"
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <future>

/**
 * @file PluginManager.h
 * @brief Gestión de carga, ejecución y ciclo de vida de plugins dinámicos.
 */

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

/**
 * @enum PluginType
 * @brief Rol funcional de un plugin dentro del ciclo de simulación.
 */
enum class PluginType {
    SEQUENTIAL_STATE_MODIFIER = 0,
    PARALLEL_PHYSICS_CALCULATOR = 1
};

// Forward declarations
namespace MoLab {
    class PhysicsIntegrator;
    class PluginTaskScheduler;
}

namespace MoLab {

/**
 * @struct LoadedPlugin
 * @brief Representa un plugin cargado con metadatos, funciones y métricas.
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
    void(*set_host_services_func)(const PluginHostServices*) = nullptr;

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
          set_host_services_func(other.set_host_services_func),
          lib_handle(other.lib_handle) {

        // Resetear el objeto origen
        other.handle = nullptr;
        other.create_func = nullptr;
        other.configure_func = nullptr;
        other.tick_func = nullptr;
        other.destroy_func = nullptr;
        other.set_host_services_func = nullptr;
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
            set_host_services_func = other.set_host_services_func;
            lib_handle = other.lib_handle;

            // Resetear el objeto origen
            other.handle = nullptr;
            other.create_func = nullptr;
            other.configure_func = nullptr;
            other.tick_func = nullptr;
            other.destroy_func = nullptr;
            other.set_host_services_func = nullptr;
            other.lib_handle = nullptr;
        }
        return *this;
    }
};

/**
 * @class PluginManager
 * @brief Gestiona el ciclo de vida y ejecución de plugins con seguridad de hilos.
 */
class PluginManager {
public:
    /**
     * @brief Construye el gestor e inicializa subsistemas auxiliares.
     */
    PluginManager();

    /**
     * @brief Destruye el gestor y libera recursos asociados.
     */
    ~PluginManager();

    // Prevenir copia y asignación
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    /**
     * @brief Carga un plugin dinámico y lo clasifica por tipo.
     * @param path Ruta de la librería del plugin.
     * @param type Tipo de plugin a registrar.
     * @return `true` si el plugin se carga correctamente.
     */
    bool load_plugin(const std::string& path, PluginType type);

    /**
     * @brief Carga plugins definidos en la configuración global.
     * @return `true` si todos los plugins habilitados se cargaron correctamente.
     */
    bool load_plugins_from_config();

    /**
     * @brief Ejecuta un ciclo completo de plugins e integración física.
     * @param state_buffer Buffer de estado serializado.
     * @param delta_time Paso temporal del tick.
     */
    void run_simulation_cycle(std::vector<uint8_t>& state_buffer, double delta_time);

    /**
     * @brief Libera todos los recursos y descarga todos los plugins.
     */
    void shutdown();

    /**
     * @brief Obtiene la cantidad de plugins cargados.
     * @return Número de plugins registrados.
     */
    size_t get_plugin_count() const;

    /**
     * @brief Obtiene nombres de plugins cargados.
     * @return Lista de nombres o rutas de plugins.
     */
    std::vector<std::string> get_loaded_plugin_names() const;

    /**
     * @struct PluginMetrics
     * @brief Métricas de ejecución acumuladas por plugin.
     */
    struct PluginMetrics {
        std::string name;
        uint64_t execution_count;
        double total_execution_time;
        double average_execution_time;
        double last_execution_time;
    };

    /**
     * @brief Obtiene métricas de rendimiento de plugins cargados.
     * @return Vector de métricas por plugin.
     */
    std::vector<PluginMetrics> get_plugin_metrics() const;

private:
    // Contenedores de plugins con protección de concurrencia
    std::vector<LoadedPlugin> loaded_plugins_;
    mutable std::mutex plugins_mutex_;

    // Métricas globales del ciclo de simulación
    std::atomic<uint64_t> total_cycles_{0};
    std::atomic<double> total_cycle_time_{0.0};

    // Métodos internos de ejecución
    void execute_sequential_plugins(std::vector<uint8_t>& state_buffer, double delta_time);
    void execute_parallel_plugins(std::vector<uint8_t>& state_buffer, double delta_time);
    void apply_physics_integration(std::vector<uint8_t>& state_buffer, double delta_time);

    // Utilidades
    std::vector<LoadedPlugin*> get_plugins_by_type(PluginType type);
    void cleanup_plugin(LoadedPlugin& plugin);

    // Integrador físico
    std::unique_ptr<MoLab::PhysicsIntegrator> physics_integrator_;

    // Scheduler de tareas para plugins paralelos
    std::unique_ptr<PluginTaskScheduler> task_scheduler_;

    // Acumuladores de fuerzas/torques (plugins paralelos)
    PluginVector3 accumulated_force_{0.0f, 0.0f, 0.0f};
    PluginVector3 accumulated_torque_{0.0f, 0.0f, 0.0f};
    mutable std::mutex force_mutex_;
};

} // namespace MoLab

#endif
