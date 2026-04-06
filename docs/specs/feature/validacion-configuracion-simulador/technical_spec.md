# Especificación Técnica — Validación de Configuración del Simulador

**Rama:** `feature/validacion-configuracion-simulador`  
**Fecha:** 2026-02-22 (Actualizado)  
**Estado:** En Revisión  
**Referencia:** [Especificación Funcional](./functional_spec.md)

---

## 1. Arquitectura del Sistema de Validación

### 1.1 Componentes Principales

```
┌─────────────────────────────────────────────────────────────┐
│                    ConfigManager                            │
│  - loadConfig()                                             │
│  - validateConfig()  ← NUEVO                                │
│  - getSimulationConfig(), getPluginConfigs()                │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────────┐
│                  ConfigValidator  ← NUEVO                   │
│  - validateStructure()                                      │
│  - validateTypes()                                          │
│  - validateRanges()                                         │
│  - validateDependencies()                                   │
│  - validatePaths()                                          │
└──────────────────┬──────────────────────────────────────────┘
                   │
                   ├──► PluginValidator  ← NUEVO
                   │    - validatePluginConfig()
                   │    - validatePluginParameters()
                   │
                   └──► ValidationReport  ← NUEVO
                        - addError(), addWarning()
                        - print(), toJSON()
```

### 1.2 Flujo de Validación

```
[ConfigManager::loadConfig(config_file)]
    │
    ├──► 1. Parsear JSON
    │
    ├──► 2. ConfigValidator::validate(json)
    │         │
    │         ├──► validateStructure()
    │         │    - Verificar secciones requeridas
    │         │    - Verificar estructura de secciones opcionales
    │         │
    │         ├──► validateTypes()
    │         │    - Verificar tipos de datos
    │         │    - Verificar enums válidos
    │         │
    │         ├──► validateRanges()
    │         │    - Verificar rangos numéricos
    │         │    - Verificar límites físicos
    │         │
    │         ├──► validatePaths()
    │         │    - Verificar existencia de archivos
    │         │    - Normalizar rutas multiplataforma
    │         │
    │         ├──► validateDependencies()
    │         │    - Verificar dependencias entre secciones
    │         │    - Verificar coherencia de configuración
    │         │
    │         └──► PluginValidator::validatePlugins()
    │              - Validar configuración de cada plugin
    │              - Validar parámetros específicos
    │
    ├──► 3. ValidationReport::hasErrors()
    │         │
    │         ├── SI: Retornar false, imprimir errores
    │         └── NO: Continuar
    │
    └──► 4. Aplicar configuración validada
```

## 2. Diseño de Clases

### 2.1 ConfigValidator

**Archivo:** `src/core/ConfigValidator.h`

```cpp
#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "ValidationReport.h"

namespace MoLab {

class ConfigValidator {
public:
    // Validar configuración completa
    static ValidationReport validate(const nlohmann::json& config);
    
private:
    // Validadores específicos
    static void validateStructure(const nlohmann::json& config, ValidationReport& report);
    static void validateSimulationSection(const nlohmann::json& sim, ValidationReport& report);
    static void validatePluginsSection(const nlohmann::json& plugins, ValidationReport& report);
    static void validateLoggingSection(const nlohmann::json& logging, ValidationReport& report);
    static void validatePerformanceSection(const nlohmann::json& perf, ValidationReport& report);
    static void validateValidationSection(const nlohmann::json& val, ValidationReport& report);
    static void validateMissionEnvironment(const nlohmann::json& env, ValidationReport& report);
    
    // Validadores de tipos
    static bool validateFloat(const nlohmann::json& value, const std::string& name, 
                             ValidationReport& report);
    static bool validateInt(const nlohmann::json& value, const std::string& name,
                           ValidationReport& report);
    static bool validateBool(const nlohmann::json& value, const std::string& name,
                            ValidationReport& report);
    static bool validateString(const nlohmann::json& value, const std::string& name,
                              ValidationReport& report);
    static bool validateEnum(const nlohmann::json& value, const std::string& name,
                            const std::vector<std::string>& allowed,
                            ValidationReport& report);
    
    // Validadores de rangos
    static bool validateRange(double value, const std::string& name,
                             double min, double max,
                             ValidationReport& report);
    
    // Validadores de rutas
    static bool validateFilePath(const std::string& path, const std::string& name,
                                bool must_exist, ValidationReport& report);
    static bool validateDirectoryPath(const std::string& path, const std::string& name,
                                     bool must_exist, ValidationReport& report);
    
    // Validadores de dependencias
    static void validateDependencies(const nlohmann::json& config, ValidationReport& report);
};

} // namespace MoLab
```

### 2.2 ValidationReport

**Archivo:** `src/core/ValidationReport.h`

```cpp
#pragma once

#include <string>
#include <vector>

namespace MoLab {

enum class ValidationLevel {
    ERROR,    // Bloquea ejecución
    WARNING,  // Permite ejecución pero reporta
    INFO      // Información adicional
};

struct ValidationMessage {
    ValidationLevel level;
    std::string section;  // ej. "simulation", "plugins[0]"
    std::string field;    // ej. "duration", "library_path"
    std::string message;  // Descripción del problema
};

class ValidationReport {
public:
    void addError(const std::string& section, const std::string& field, 
                  const std::string& message);
    void addWarning(const std::string& section, const std::string& field,
                   const std::string& message);
    void addInfo(const std::string& section, const std::string& field,
                const std::string& message);
    
    bool hasErrors() const { return error_count_ > 0; }
    bool hasWarnings() const { return warning_count_ > 0; }
    
    int getErrorCount() const { return error_count_; }
    int getWarningCount() const { return warning_count_; }
    
    // Imprimir reporte a consola (con colores)
    void print() const;
    
    // Exportar a JSON
    std::string toJSON() const;
    
    // Obtener mensajes
    const std::vector<ValidationMessage>& getMessages() const { return messages_; }
    
private:
    std::vector<ValidationMessage> messages_;
    int error_count_ = 0;
    int warning_count_ = 0;
    int info_count_ = 0;
};

} // namespace MoLab
```

### 2.3 PluginValidator

**Archivo:** `src/core/PluginValidator.h`

```cpp
#pragma once

#include <nlohmann/json.hpp>
#include "ValidationReport.h"

namespace MoLab {

class PluginValidator {
public:
    // Validar configuración de un plugin
    static void validatePlugin(const nlohmann::json& plugin, int index,
                              ValidationReport& report);
    
private:
    // Validadores específicos por tipo de plugin
    static void validateAerodynamicsPlugin(const nlohmann::json& params,
                                          const std::string& section,
                                          ValidationReport& report);
    static void validatePropulsionPlugin(const nlohmann::json& params,
                                        const std::string& section,
                                        ValidationReport& report);
    static void validateEnvironmentPlugin(const nlohmann::json& params,
                                         const std::string& section,
                                         ValidationReport& report);
    static void validateStructuresPlugin(const nlohmann::json& params,
                                        const std::string& section,
                                        ValidationReport& report);
    
    // Normalizar library_path multiplataforma
    static std::string normalizeLibraryPath(const std::string& path);
};

} // namespace MoLab
```

## 3. Implementación Detallada

### 3.1 Validación de Estructura

**Archivo:** `src/core/ConfigValidator.cpp`

```cpp
void ConfigValidator::validateStructure(const nlohmann::json& config, 
                                       ValidationReport& report) {
    // Secciones requeridas
    if (!config.contains("simulation")) {
        report.addError("root", "simulation", 
                       "Missing required section 'simulation'");
    }
    
    if (!config.contains("plugins")) {
        report.addError("root", "plugins",
                       "Missing required section 'plugins'");
    }
    
    // Secciones opcionales - validar estructura si existen
    if (config.contains("logging") && !config["logging"].is_object()) {
        report.addError("root", "logging",
                       "Section 'logging' must be an object");
    }
    
    if (config.contains("plugins") && !config["plugins"].is_array()) {
        report.addError("root", "plugins",
                       "Section 'plugins' must be an array");
    }
    
    // ... más validaciones de estructura
}
```

### 3.2 Validación de Tipos y Rangos

```cpp
void ConfigValidator::validateSimulationSection(const nlohmann::json& sim,
                                               ValidationReport& report) {
    const std::string section = "simulation";
    
    // duration: float > 0, rango [0.001, 86400.0]
    if (validateFloat(sim["duration"], "duration", report)) {
        double duration = sim["duration"];
        validateRange(duration, section + ".duration", 
                     0.001, 86400.0, report);
    }
    
    // max_iterations: int > 0, rango [1, 1000000]
    if (validateInt(sim["max_iterations"], "max_iterations", report)) {
        int max_iter = sim["max_iterations"];
        if (max_iter < 1 || max_iter > 1000000) {
            report.addError(section, "max_iterations",
                           "Must be in range [1, 1000000], got " + 
                           std::to_string(max_iter));
        }
    }
    
    // log_level: enum
    std::vector<std::string> valid_levels = 
        {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
    validateEnum(sim["log_level"], "log_level", valid_levels, report);
}
```

### 3.3 Validación de Dependencias

```cpp
void ConfigValidator::validateDependencies(const nlohmann::json& config,
                                          ValidationReport& report) {
    // Si logging.file_output = true, log_file debe estar especificado
    if (config.contains("logging")) {
        auto& logging = config["logging"];
        if (logging.value("file_output", false)) {
            if (!config["simulation"].contains("log_file") ||
                config["simulation"]["log_file"].empty()) {
                report.addError("logging", "file_output",
                               "file_output=true requires simulation.log_file");
            }
        }
    }
    
    // Si performance.enable_profiling = true, enable_metrics debe ser true
    if (config.contains("performance")) {
        auto& perf = config["performance"];
        if (perf.value("enable_profiling", false)) {
            if (!perf.value("enable_metrics", false)) {
                report.addError("performance", "enable_profiling",
                               "enable_profiling requires enable_metrics=true");
            }
        }
    }
    
    // Plugins enabled deben tener library_path válido
    if (config.contains("plugins")) {
        for (size_t i = 0; i < config["plugins"].size(); ++i) {
            auto& plugin = config["plugins"][i];
            if (plugin.value("enabled", false)) {
                if (!plugin.contains("library_path") ||
                    plugin["library_path"].empty()) {
                    std::string section = "plugins[" + std::to_string(i) + "]";
                    report.addError(section, "library_path",
                                   "Enabled plugin must have library_path");
                }
            }
        }
    }
}
```

### 3.4 Validación de Plugins

**Archivo:** `src/core/PluginValidator.cpp`

```cpp
void PluginValidator::validatePlugin(const nlohmann::json& plugin, int index,
                                    ValidationReport& report) {
    std::string section = "plugins[" + std::to_string(index) + "]";
    
    // Validar campos básicos
    if (!plugin.contains("name") || plugin["name"].empty()) {
        report.addError(section, "name", "Plugin name is required");
    }
    
    if (!plugin.contains("type")) {
        report.addError(section, "type", "Plugin type is required");
    } else {
        int type = plugin["type"];
        if (type < 0 || type > 1) {
            report.addError(section, "type",
                           "Plugin type must be 0 or 1, got " + 
                           std::to_string(type));
        }
    }
    
    // Normalizar library_path
    if (plugin.contains("library_path")) {
        std::string path = plugin["library_path"];
        // Verificar que no tenga extensión específica de plataforma
        if (path.find(".dll") != std::string::npos ||
            path.find(".dylib") != std::string::npos ||
            path.find(".so") != std::string::npos) {
            report.addWarning(section, "library_path",
                             "Library path should not include platform-specific extension. "
                             "Use base name only (e.g., 'build/lib/libplugin')");
        }
    }
    
    // Validar parámetros específicos según nombre del plugin
    if (plugin.contains("parameters")) {
        std::string name = plugin["name"];
        if (name == "aerodynamics") {
            validateAerodynamicsPlugin(plugin["parameters"], section, report);
        } else if (name == "propulsion") {
            validatePropulsionPlugin(plugin["parameters"], section, report);
        } else if (name == "environment") {
            validateEnvironmentPlugin(plugin["parameters"], section, report);
        } else if (name == "structures") {
            validateStructuresPlugin(plugin["parameters"], section, report);
        }
    }
}

void PluginValidator::validateAerodynamicsPlugin(const nlohmann::json& params,
                                                const std::string& section,
                                                ValidationReport& report) {
    if (params.contains("reference_area")) {
        double area = params["reference_area"];
        if (area <= 0) {
            report.addError(section, "parameters.reference_area",
                           "Must be > 0, got " + std::to_string(area));
        }
    }
    
    if (params.contains("drag_coefficient")) {
        double cd = params["drag_coefficient"];
        if (cd < 0) {
            report.addError(section, "parameters.drag_coefficient",
                           "Must be >= 0, got " + std::to_string(cd));
        }
    }
}
```

## 4. Integración con ConfigManager

**Archivo:** `src/core/ConfigManager.cpp`

```cpp
bool ConfigManager::loadConfig(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            LOG_WARNING("Config file not found, using defaults: " + config_file, 
                       "ConfigManager");
            setDefaults();
            return false;
        }

        nlohmann::json config_json;
        file >> config_json;
        file.close();

        LOG_INFO("Loading configuration from: " + config_file, "ConfigManager");

        // ===== NUEVO: Validar configuración antes de parsear =====
        ValidationReport report = ConfigValidator::validate(config_json);
        
        if (report.hasErrors()) {
            LOG_ERROR("Configuration validation failed:", "ConfigManager");
            report.print();  // Imprimir errores a consola
            return false;
        }
        
        if (report.hasWarnings()) {
            LOG_WARNING("Configuration has warnings:", "ConfigManager");
            report.print();  // Imprimir warnings
        }
        // =========================================================

        // Parsear secciones (ahora sabemos que son válidas)
        if (!parseSimulationConfig(config_json)) {
            return false;
        }
        if (!parsePluginConfigs(config_json)) {
            return false;
        }

        // ... resto del código
        
        LOG_INFO("Configuration loaded successfully", "ConfigManager");
        return true;

    } catch (const std::exception& e) {
        LOG_ERROR("Error loading config: " + std::string(e.what()), 
                 "ConfigManager");
        setDefaults();
        return false;
    }
}
```

## 5. Ejemplo de Reporte de Validación

### 5.1 Configuración Inválida

```json
{
  "simulation": {
    "duration": -10.0,
    "max_iterations": 2000000,
    "log_level": "TRACE"
  },
  "plugins": [
    {
      "name": "aerodynamics",
      "type": 5,
      "library_path": "build/lib/libaero.dylib",
      "enabled": true,
      "parameters": {
        "reference_area": -5.0
      }
    }
  ]
}
```

### 5.2 Salida del Validador

```
╔══════════════════════════════════════════════════════════════╗
║         CONFIGURATION VALIDATION REPORT                      ║
╚══════════════════════════════════════════════════════════════╝

❌ ERRORS (4):

  [simulation.duration]
    Must be > 0, got -10.0
  
  [simulation.max_iterations]
    Must be in range [1, 1000000], got 2000000
  
  [simulation.log_level]
    Invalid value 'TRACE'. Allowed: DEBUG, INFO, WARNING, ERROR, CRITICAL
  
  [plugins[0].type]
    Plugin type must be 0 or 1, got 5
  
  [plugins[0].parameters.reference_area]
    Must be > 0, got -5.0

⚠️  WARNINGS (1):

  [plugins[0].library_path]
    Library path should not include platform-specific extension.
    Use base name only (e.g., 'build/lib/libplugin')

───────────────────────────────────────────────────────────────
SUMMARY: 4 errors, 1 warning
STATUS: ❌ VALIDATION FAILED - Cannot start simulation
───────────────────────────────────────────────────────────────
```

## 6. Testing

### 6.1 Tests Unitarios

**Archivo:** `tests/config_validation_test.cpp`

```cpp
#include <gtest/gtest.h>
#include "ConfigValidator.h"
#include "ValidationReport.h"

using namespace MoLab;

TEST(ConfigValidatorTest, ValidConfiguration) {
    nlohmann::json config = {
        {"simulation", {
            {"duration", 100.0},
            {"max_iterations", 1000},
            {"enable_logging", true},
            {"log_file", "logs/test.log"},
            {"log_level", "INFO"}
        }},
        {"plugins", nlohmann::json::array()}
    };
    
    ValidationReport report = ConfigValidator::validate(config);
    
    EXPECT_FALSE(report.hasErrors());
    EXPECT_EQ(report.getErrorCount(), 0);
}

TEST(ConfigValidatorTest, MissingRequiredSection) {
    nlohmann::json config = {
        {"plugins", nlohmann::json::array()}
        // Missing 'simulation' section
    };
    
    ValidationReport report = ConfigValidator::validate(config);
    
    EXPECT_TRUE(report.hasErrors());
    EXPECT_GT(report.getErrorCount(), 0);
}

TEST(ConfigValidatorTest, InvalidDuration) {
    nlohmann::json config = {
        {"simulation", {
            {"duration", -10.0},  // Invalid: negative
            {"max_iterations", 1000}
        }},
        {"plugins", nlohmann::json::array()}
    };
    
    ValidationReport report = ConfigValidator::validate(config);
    
    EXPECT_TRUE(report.hasErrors());
}

TEST(ConfigValidatorTest, InvalidLogLevel) {
    nlohmann::json config = {
        {"simulation", {
            {"duration", 100.0},
            {"max_iterations", 1000},
            {"log_level", "TRACE"}  // Invalid enum value
        }},
        {"plugins", nlohmann::json::array()}
    };
    
    ValidationReport report = ConfigValidator::validate(config);
    
    EXPECT_TRUE(report.hasErrors());
}

TEST(ConfigValidatorTest, PluginLibraryPathNormalization) {
    nlohmann::json config = {
        {"simulation", {{"duration", 100.0}, {"max_iterations", 1000}}},
        {"plugins", {
            {
                {"name", "test"},
                {"type", 0},
                {"library_path", "build/lib/libtest.dylib"},  // Platform-specific
                {"enabled", true}
            }
        }}
    };
    
    ValidationReport report = ConfigValidator::validate(config);
    
    EXPECT_FALSE(report.hasErrors());
    EXPECT_TRUE(report.hasWarnings());  // Should warn about .dylib
}
```

## 7. Archivos a Crear/Modificar

| Acción | Archivo | Descripción |
|--------|---------|-------------|
| **CREAR** | `src/core/ConfigValidator.h` | Clase principal de validación |
| **CREAR** | `src/core/ConfigValidator.cpp` | Implementación de validadores |
| **CREAR** | `src/core/ValidationReport.h` | Clase para reportes de validación |
| **CREAR** | `src/core/ValidationReport.cpp` | Implementación de reportes |
| **CREAR** | `src/core/PluginValidator.h` | Validador específico de plugins |
| **CREAR** | `src/core/PluginValidator.cpp` | Implementación validación plugins |
| **CREAR** | `tests/config_validation_test.cpp` | Tests unitarios |
| **CREAR** | `data/schemas/config_schema.json` | Esquema JSON formal (opcional) |
| **CREAR** | `data/examples/configs/valid_example.json` | Ejemplo de config válida |
| **CREAR** | `data/examples/configs/invalid_example.json` | Ejemplo de config inválida |
| **MODIFICAR** | `src/core/ConfigManager.h` | Agregar método validateConfig() |
| **MODIFICAR** | `src/core/ConfigManager.cpp` | Integrar validación en loadConfig() |
| **MODIFICAR** | `CMakeLists.txt` | Agregar nuevos archivos al build |

## 8. Cronograma de Implementación

### Fase 1: Infraestructura Básica (2-3 días)
- Crear ValidationReport
- Crear ConfigValidator con validación de estructura
- Integrar en ConfigManager
- Tests básicos

### Fase 2: Validación Completa (3-4 días)
- Implementar validación de tipos
- Implementar validación de rangos
- Implementar validación de rutas
- Implementar validación de dependencias
- Tests completos

### Fase 3: Validación de Plugins (2-3 días)
- Crear PluginValidator
- Implementar validadores específicos por plugin
- Normalización de rutas multiplataforma
- Tests de plugins

### Fase 4: Documentación y Ejemplos (1-2 días)
- Crear esquema JSON
- Crear ejemplos de configuraciones
- Documentar guía de configuración
- Actualizar README

**Total estimado: 8-12 días**

```cpp
// core/PluginManager.cpp:309
total_force = total_force + Vector3(0.0, 0.0, -9.81 * physics_state.mass);
```

**Problema:** Siempre aplica -9.81 m/s² sin consultar `PhysicsConfig::enable_gravity`.  
**Impacto:** Si el usuario desactiva gravedad en la web, el simulador la aplica igual.  
**Doble gravedad:** Si el plugin `environment` está cargado, este TAMBIÉN calcula gravedad (línea 432 de `environment.cpp`), resultando en ~2× la gravedad real.

#### P2: `gravity_magnitude` no existe en C++

```cpp
// src/core/ConfigManager.h:27-33
struct PhysicsConfig {
    bool enable_gravity;                // ← solo un bool
    bool enable_atmospheric_drag;
    bool enable_wind_effects;
    double integration_tolerance;
    std::string integrator_type;
};
```

**Problema:** La web envía `gravity_magnitude: 9.81` pero C++ no tiene ese campo. El valor se pierde al parsear el JSON.

#### P3: Integrador nunca se configura desde config

```cpp
// core/PluginManager.cpp:22
PluginManager::PluginManager() : physics_integrator_(std::make_unique<PhysicsIntegrator>()) {
```

**Problema:** El constructor de `PhysicsIntegrator` usa default `IntegratorType::RUNGE_KUTTA_4`. Nunca se llama a `setIntegratorType(physics_config.integrator_type)`.

#### P4: Condiciones atmosféricas estáticas

```cpp
// core/PluginManager.cpp:336-346
auto general_state = state_vector::CreateGeneralState(builder,
    &position, &velocity, &orientation,
    current_state->atm_density(),        // ← COPIA el valor anterior sin cambiar
    current_state->atm_pressure(),       // ← COPIA el valor anterior sin cambiar
    current_state->atm_temperature(),    // ← COPIA el valor anterior sin cambiar
    &gravity, ...);
```

**Problema:** Los valores atmosféricos se copian del tick anterior sin recalcular. La función `AtmosphericEffects::calculateAirDensity()` existe en `PhysicsIntegrator.cpp:274` pero nunca se llama.

#### P5: `enable_atmospheric_drag` ignorado

**Problema:** `PhysicsConfig::enable_atmospheric_drag` se lee pero nunca se consulta en el loop de simulación. Solo los plugins (aerodynamics) aplican drag si están cargados.

#### P6-P7: Output config hardcodeado

```cpp
// core/SimulationEngine.cpp:113-115
output_manager.setOutputDirectory("output");              // ❌ Ignora config
output_manager.setOutputFormats(true, true, false);       // ❌ Ignora config
output_manager.setOutputInterval(5);                      // ❌ Ignora config
```

#### P8: Estado inicial hardcodeado en web

```python
# tools/molab_web_gui.py:2746
config["initial_state_file"] = str(self.project_root / "data" / "initial_state" / "realistic_state.json")
```

**Problema:** Siempre usa el mismo archivo de estado inicial, sin importar si la web envía uno diferente.

#### P9: Frontend con datos fake

```javascript
// tools/molab_web_gui.py (JS embebido, línea ~1010)
const simulatedPosition = {
    x: (100 * currentTick * timeStep).toFixed(1),
    y: 0,
    z: (5000 + 50 * currentTick * timeStep - 4.9 * Math.pow(currentTick * timeStep, 2)).toFixed(1)
};
```

**Problema:** El frontend muestra posición/velocidad calculada con fórmulas JavaScript fake, no datos reales del simulador.

## 9. Casos de Prueba

### 2.1 Cambios en C++ (Motor de Simulación)

#### 2.1.1 Extender `PhysicsConfig` con `gravity_magnitude`

**Archivo:** `src/core/ConfigManager.h`

```cpp
struct PhysicsConfig {
    bool enable_gravity;
    double gravity_magnitude;          // NUEVO: valor configurable (default: 9.81)
    bool enable_atmospheric_drag;
    bool enable_wind_effects;
    double integration_tolerance;
    std::string integrator_type;
};
```

**Archivo:** `src/core/ConfigManager.cpp` — `parsePhysicsConfig()` y `setDefaults()`

Agregar parseo de `gravity_magnitude` con default 9.81.

#### 2.1.2 Corregir `apply_physics_integration()` para usar config

**Archivo:** `core/PluginManager.cpp` — `apply_physics_integration()`

```cpp
// ANTES (línea 309):
total_force = total_force + Vector3(0.0, 0.0, -9.81 * physics_state.mass);

// DESPUÉS:
const auto& physics_config = ConfigManager::getInstance().getPhysicsConfig();
if (physics_config.enable_gravity) {
    double g = physics_config.gravity_magnitude;
    total_force = total_force + Vector3(0.0, 0.0, -g * physics_state.mass);
}
```

#### 2.1.3 Aplicar integrador desde config

**Archivo:** `core/PluginManager.cpp` — `run_simulation_cycle_improved()` o constructor

En `load_plugins_from_config()` o al inicio de `run_simulation_cycle_improved()`, configurar el integrador:

```cpp
const auto& physics_config = ConfigManager::getInstance().getPhysicsConfig();
physics_integrator_->setIntegratorType(physics_config.integrator_type);
```

#### 2.1.4 Actualizar condiciones atmosféricas dinámicamente

**Archivo:** `core/PluginManager.cpp` — `apply_physics_integration()`

Después de integrar la posición, recalcular condiciones atmosféricas basándose en la altitud:

```cpp
double altitude = new_state.position.z;  // para coordenadas locales
float new_density = AtmosphericEffects::calculateAirDensity(altitude);
// ... calcular presión y temperatura también
```

Y usar estos valores al crear el nuevo `GeneralState`.

#### 2.1.5 Aplicar drag atmosférico desde config

**Archivo:** `core/PluginManager.cpp` — `apply_physics_integration()`

Si `enable_atmospheric_drag = true`, calcular y aplicar fuerza de drag:

```cpp
if (physics_config.enable_atmospheric_drag) {
    double air_density = AtmosphericEffects::calculateAirDensity(altitude);
    Vector3 drag = AtmosphericEffects::calculateDrag(
        physics_state.velocity, air_density, 0.47, 1.0);
    total_force = total_force + drag;
}
```

#### 2.1.6 Propagar output config

**Archivo:** `core/SimulationEngine.cpp` — `initialize_with_config()`

Leer y usar la configuración de output del JSON:

```cpp
if (config_json.contains("output")) {
    auto& out = config_json["output"];
    output_manager.setOutputDirectory(out.value("output_directory", "output"));
    output_manager.setOutputInterval(out.value("output_interval", 5));
    output_manager.setOutputFormats(
        out.value("enable_csv", true),
        out.value("enable_json", true),
        out.value("enable_binary", false));
}
```

**Nota:** Esto requiere que `ConfigManager` parsee y exponga la sección `output`, o que `SimulationEngine` lea el JSON directamente.

### 2.2 Cambios en Python (Web Server)

#### 2.2.1 No hardcodear initial_state_file

**Archivo:** `tools/molab_web_gui.py` — `handle_run_simulation()`

Permitir que la web envíe `initial_state_file` o usar un default sensato sin hardcodear.

#### 2.2.2 Eliminar datos fake del frontend

**Archivo:** `tools/molab_web_gui.py` — JS embebido

Eliminar las fórmulas JavaScript que simulan posición/velocidad en `monitorSimulationProgress()`. Simplificar a solo una barra de progreso basada en tiempo estimado, sin datos de posición fake.

### 2.3 Limpieza de código

| Archivo                          | Acción                                                              |
|----------------------------------|---------------------------------------------------------------------|
| `src/core/PhysicsIntegrator.cpp` | Eliminar modelo atmosférico duplicado (ya existe en `environment.cpp`) |
| `core/PluginManager.cpp`         | Eliminar gravedad hardcodeada, usar config                          |
| `core/SimulationEngine.cpp`      | Eliminar hardcoding de output, usar config                          |
| `tools/molab_web_gui.py`         | Eliminar datos de posición/velocidad fake del frontend              |

### 9.1 Casos de Prueba Automatizados

| Acción        | Archivo                           | Cambios                                                    |
|---------------|-----------------------------------|------------------------------------------------------------|
| **Modificar** | `src/core/ConfigManager.h`        | Agregar `gravity_magnitude` a `PhysicsConfig`              |
| **Modificar** | `src/core/ConfigManager.cpp`      | Parsear `gravity_magnitude`, agregar a defaults y save     |
| **Modificar** | `core/PluginManager.cpp`          | Usar config para gravedad, integrador, drag, atmosfera     |
| **Modificar** | `core/SimulationEngine.cpp`       | Propagar output config (directory, interval, formats)      |
| **Modificar** | `tools/molab_web_gui.py`          | Eliminar datos fake, no hardcodear initial_state           |

### 9.2 Casos de Prueba Manuales

```bash
# Test 1: Configuración válida completa
cp data/examples/configs/valid_example.json data/config/test.json
./build/bin/simulator --config data/config/test.json --ticks 100
# Esperado: "Configuration loaded successfully", simulación ejecuta

# Test 2: Configuración con errores
cp data/examples/configs/invalid_example.json data/config/test.json
./build/bin/simulator --config data/config/test.json
# Esperado: Reporte de errores, simulación NO ejecuta

# Test 3: Configuración con warnings
# Crear config con library_path usando .dylib
./build/bin/simulator --config data/config/test.json --ticks 100
# Esperado: Warnings impresos, simulación ejecuta

# Test 4: Configuración parcial (solo secciones requeridas)
# Crear config solo con simulation y plugins
./build/bin/simulator --config data/config/minimal.json --ticks 100
# Esperado: Defaults aplicados, simulación ejecuta
```

## 10. Riesgos y Mitigación

| Riesgo                                               | Mitigación                                                    |
|------------------------------------------------------|---------------------------------------------------------------|
| Doble gravedad con plugin environment                | Documentar: si el plugin environment está activo, desactivar gravedad del motor (`enable_gravity = false`) |
| Inestabilidad numérica al cambiar integrador         | Validar estado cada 50 ticks (ya existe)                      |
| Ruptura de configuraciones JSON existentes           | Usar `value()` con defaults para nuevos campos                |
| Output directory no existe                           | Crear directorio automáticamente si no existe                 |

## 11. Notas de Implementación

### 11.1 Orden de Validación

Es crítico validar en este orden:
1. **Estructura** - Antes de acceder a campos
2. **Tipos** - Antes de validar rangos
3. **Rangos** - Antes de validar dependencias
4. **Rutas** - Puede hacerse en paralelo con rangos
5. **Dependencias** - Al final, cuando todos los campos son válidos
6. **Plugins** - Al final, validación específica

### 11.2 Manejo de Errores vs Warnings

**Errores (bloquean ejecución):**
- Tipos de datos incorrectos
- Rangos fuera de límites físicos
- Secciones requeridas faltantes
- Dependencias no satisfechas
- Archivos requeridos inexistentes

**Warnings (permiten ejecución):**
- Rutas con extensiones específicas de plataforma
- Valores subóptimos pero válidos
- Secciones opcionales faltantes (se usan defaults)
- Archivos opcionales inexistentes

### 11.3 Compatibilidad Retroactiva

Para mantener compatibilidad con configuraciones antiguas:

```cpp
// Si un campo opcional no existe, usar default sin error
if (!config.contains("validation")) {
    report.addInfo("root", "validation",
                   "Using default validation settings");
    // Aplicar defaults, no error
}

// Si un campo cambió de nombre, soportar ambos
if (config.contains("time_step") || config.contains("timestep")) {
    // Aceptar ambos nombres
}
```

## 12. Testing Manual

```bash
# Test 1: Gravedad desactivada — verificar trayectoria sin caída
# Configurar enable_gravity = false en la web, ejecutar simulación
# Resultado esperado: position_z no decrece

# Test 2: Gravedad de Marte (3.71 m/s²)
# Configurar gravity_magnitude = 3.71 en la web
# Resultado esperado: caída más lenta que con 9.81

# Test 3: Cambiar integrador
# Ejecutar misma config con euler vs runge_kutta_4
# Resultado esperado: valores numéricos diferentes

# Test 4: Condiciones atmosféricas dinámicas
# Ejecutar simulación con altitud variable
# Resultado esperado: atm_density, atm_pressure, atm_temperature cambian en el CSV

# Test 5: Output interval
# Configurar output_interval = 10 en la web
# Resultado esperado: CSV tiene datos cada 10 ticks (no cada 5)

# Test 6: Sin datos fake en frontend
# Ejecutar simulación y verificar que la barra de progreso no muestra posición/velocidad fake
```
