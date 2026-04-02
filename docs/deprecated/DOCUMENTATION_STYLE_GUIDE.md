# Guía de Estilo de Documentación - MoLab

## Introducción

Este documento establece el estándar oficial para la documentación dentro del código fuente del proyecto MoLab. Todo el equipo debe seguir estas convenciones para mantener consistencia y facilitar la generación automática de documentación.

## Herramienta Oficial: Doxygen

MoLab utiliza **Doxygen** como herramienta de documentación. Doxygen lee comentarios especiales en el código y genera documentación HTML, PDF o LaTeX automáticamente.

### Instalación de Doxygen

**Windows:**
```bash
# Usando winget
winget install doxygen

# O descargar desde: https://www.doxygen.nl/download.html
```

**Linux:**
```bash
sudo apt install doxygen doxygen-gui graphviz
```

**macOS:**
```bash
brew install doxygen graphviz
```

### Generar la Documentación

Desde la raíz del proyecto:
```bash
doxygen Doxyfile
```

La documentación se generará en `docs/html/index.html`.

---

## Formato de Comentarios

### Estilo Oficial: Javadoc con `/**`

Usamos el estilo Javadoc con doble asterisco para todos los comentarios de documentación:

```cpp
/**
 * @brief Descripción corta de una línea.
 *
 * Descripción más detallada del elemento. Puede ocupar
 * múltiples líneas y explicar el comportamiento en profundidad.
 */
```

### Idioma

- **Comentarios de documentación**: Español
- **Nombres de variables, clases, métodos**: Inglés (como el código existente)

---

## Documentación de Clases

Toda clase pública debe tener un bloque de documentación antes de su declaración.

### Plantilla

```cpp
/**
 * @class NombreClase
 * @brief Descripción breve de la clase.
 *
 * Descripción detallada de la responsabilidad de la clase,
 * su propósito en el sistema y cómo se relaciona con otros componentes.
 *
 * @author Nombre del autor (opcional)
 * @date Fecha de creación (opcional)
 *
 * @see ClaseRelacionada
 */
class NombreClase {
    // ...
};
```

### Ejemplo Real (SimulationEngine)

```cpp
/**
 * @class SimulationEngine
 * @brief Motor principal de simulación aeroespacial.
 *
 * Orquesta la carga del estado inicial, la ejecución de ticks
 * de simulación y la gestión de plugins de física. Implementa
 * un diseño thread-safe para permitir ejecución concurrente
 * de múltiples plugins de física.
 *
 * @see PluginManager
 * @see PhysicsIntegrator
 */
class SimulationEngine {
    // ...
};
```

---

## Documentación de Métodos y Funciones

### Plantilla

```cpp
/**
 * @brief Descripción breve del método.
 *
 * Descripción detallada del comportamiento, incluyendo
 * casos especiales y efectos secundarios.
 *
 * @param nombre_parametro Descripción del parámetro.
 * @param otro_parametro Descripción de otro parámetro.
 *
 * @return Descripción de lo que retorna.
 *
 * @throws TipoExcepcion Cuándo se lanza esta excepción.
 *
 * @pre Precondición que debe cumplirse antes de llamar.
 * @post Postcondición garantizada después de ejecutar.
 *
 * @note Notas adicionales importantes.
 * @warning Advertencias sobre uso incorrecto.
 *
 * @code
 * // Ejemplo de uso
 * engine.initialize("config.json");
 * @endcode
 */
```

### Ejemplo Real

```cpp
/**
 * @brief Inicializa la simulación desde un archivo de configuración.
 *
 * Carga la configuración JSON, configura el sistema de logging,
 * inicializa los plugins especificados y prepara el estado inicial
 * del vehículo.
 *
 * @param config_filepath Ruta al archivo de configuración JSON.
 *
 * @return true si la inicialización fue exitosa, false en caso contrario.
 *
 * @pre El archivo debe existir y ser un JSON válido.
 * @post El motor está listo para ejecutar ticks de simulación.
 *
 * @note Si el archivo no existe, se usará la configuración por defecto.
 *
 * @code
 * SimulationEngine engine;
 * if (engine.initialize_with_config("data/defaults/default_config.json")) {
 *     engine.run_simulation();
 * }
 * @endcode
 */
bool initialize_with_config(const std::string& config_filepath);
```

---

## Documentación de Variables Miembro

### Variables Privadas

```cpp
private:
    std::unique_ptr<PluginManager> plugin_manager_;  ///< Gestor de plugins dinámicos.
    std::vector<uint8_t> current_state_buffer_;      ///< Buffer del estado actual del vehículo.
    std::atomic<double> simulation_time_{0.0};       ///< Tiempo de simulación en segundos.
    std::atomic<bool> is_running_{false};            ///< Indica si la simulación está activa.
```

### Variables Públicas o Constantes

```cpp
/**
 * @brief Gravedad estándar de la Tierra.
 *
 * Valor de aceleración gravitacional al nivel del mar
 * según el modelo WGS84.
 */
static constexpr double EARTH_GRAVITY = 9.80665;
```

---

## Documentación de Namespaces

```cpp
/**
 * @namespace MoLab
 * @brief Espacio de nombres principal del simulador aeroespacial.
 *
 * Contiene todas las clases, funciones y constantes del motor
 * de simulación MoLab.
 */
namespace MoLab {
    // ...
}
```

---

## Documentación de Enums

```cpp
/**
 * @enum IntegratorType
 * @brief Tipos de integradores numéricos disponibles.
 */
enum class IntegratorType {
    EULER,          ///< Integrador Euler simple (primer orden).
    RUNGE_KUTTA_4,  ///< Integrador Runge-Kutta de cuarto orden.
    VERLET          ///< Integrador Verlet (conservación de energía).
};
```

---

## Documentación de Archivos

Al inicio de cada archivo `.cpp` y `.h`:

```cpp
/**
 * @file SimulationEngine.cpp
 * @brief Implementación del motor principal de simulación.
 *
 * @author Equipo MoLab
 * @date 2024
 *
 * Este archivo contiene la implementación de la clase SimulationEngine,
 * responsable de orquestar toda la simulación aeroespacial.
 */
```

---

## Documentación de Plugins

Los plugins siguen el mismo estándar pero deben incluir información adicional:

```cpp
/**
 * @file example_plugin.cpp
 * @brief Plugin de ejemplo para demostrar la API de plugins.
 *
 * @plugin_name example_plugin
 * @plugin_type 0 (Secuencial)
 *
 * Este plugin demuestra cómo implementar la interfaz de plugins
 * de MoLab. Modifica la posición X del vehículo en cada tick.
 *
 * @see plugin_api.h
 */
```

---

## Comandos Doxygen de Referencia Rápida

| Comando | Uso |
|---------|-----|
| `@brief` | Descripción corta (una línea) |
| `@param` | Documentar un parámetro |
| `@return` | Documentar el valor de retorno |
| `@throws` | Documentar excepciones |
| `@see` | Referencia a elementos relacionados |
| `@note` | Nota informativa |
| `@warning` | Advertencia importante |
| `@deprecated` | Marcar como obsoleto |
| `@todo` | Tareas pendientes |
| `@bug` | Bugs conocidos |
| `@code` / `@endcode` | Bloque de código de ejemplo |
| `@pre` | Precondición |
| `@post` | Postcondición |
| `///&lt;` | Comentario corto después de variable |

---

## Lista de Verificación

Antes de hacer commit, verifica:

- [ ] ¿Todas las clases públicas tienen `@brief` y descripción?
- [ ] ¿Todos los métodos públicos tienen `@param` y `@return`?
- [ ] ¿Los archivos nuevos tienen el bloque `@file`?
- [ ] ¿Los comentarios están en español?
- [ ] ¿Se ejecutó `doxygen Doxyfile` sin warnings?

---

## Generación de Documentación en CI/CD

La documentación se genera automáticamente en cada push a `main`. Ver `.github/workflows/ci.yml` para detalles.

---

## Recursos Adicionales

- [Manual Oficial de Doxygen](https://www.doxygen.nl/manual/)
- [Doxygen Quick Reference](https://www.doxygen.nl/manual/commands.html)
- [Ejemplo de Proyecto Bien Documentado](https://github.com/opencv/opencv)

---

*Última actualización: Febrero 2026*
*Versión: 1.0*
