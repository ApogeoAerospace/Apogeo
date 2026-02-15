# Especificación Técnica — Validación de Configuración del Simulador

**Rama:** `feature/validacion-configuracion-simulador`  
**Fecha:** 2026-02-12  
**Estado:** Borrador — Pendiente de revisión  
**Referencia:** [Especificación Funcional](./functional_spec.md)

---

## 1. Diagnóstico Detallado

### 1.1 Flujo actual de configuración (Web → Simulador)

```
[Web UI] getConfigFromForm()
    │
    ▼ POST /api/run (JSON)
[molab_web_gui.py] handle_run_simulation()
    │ ── hardcodea initial_state_file = "realistic_state.json"
    │ ── escribe temp_config JSON
    │
    ▼ subprocess: simulator.exe --config temp_config.json --ticks N
[main.cpp]
    │ ── engine.initialize_with_config(config_file)
    │
    ▼
[ConfigManager.cpp] loadConfig()
    │ ── parseSimulationConfig()  ✅ Lee time_step, duration
    │ ── parsePhysicsConfig()     ⚠️  Lee enable_gravity, integrator_type
    │ ── parsePluginConfigs()     ✅ Lee plugins
    │
    ▼
[SimulationEngine.cpp] initialize_with_config()
    │ ── ❌ hardcodea output_directory = "output"
    │ ── ❌ hardcodea output_interval = 5
    │ ── load_plugins_from_config()  ✅
    │ ── initialize(initial_state_file)
    │
    ▼
[SimulationEngine.cpp] run_tick()
    │ ── lee config.time_step  ✅
    │ ── plugin_manager_->run_simulation_cycle_improved()
    │
    ▼
[PluginManager.cpp] run_simulation_cycle_improved()
    │ ── execute_sequential_plugins()
    │ ── execute_parallel_plugins()    → acumula fuerzas de plugins
    │ ── apply_physics_integration()
    │       │
    │       ├── ❌ SIEMPRE suma gravedad: Vector3(0, 0, -9.81 * mass)
    │       ├── ❌ NUNCA consulta PhysicsConfig
    │       ├── ❌ NUNCA aplica drag atmosférico
    │       ├── ❌ NUNCA actualiza atm_density/pressure/temperature
    │       └── ❌ Integrador SIEMPRE es RK4 (default del constructor)
```

### 1.2 Problemas identificados con ubicación exacta

#### P1: Gravedad hardcodeada

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

## 2. Plan de Corrección

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

## 3. Archivos a Crear/Modificar

| Acción        | Archivo                           | Cambios                                                    |
|---------------|-----------------------------------|------------------------------------------------------------|
| **Modificar** | `src/core/ConfigManager.h`        | Agregar `gravity_magnitude` a `PhysicsConfig`              |
| **Modificar** | `src/core/ConfigManager.cpp`      | Parsear `gravity_magnitude`, agregar a defaults y save     |
| **Modificar** | `core/PluginManager.cpp`          | Usar config para gravedad, integrador, drag, atmosfera     |
| **Modificar** | `core/SimulationEngine.cpp`       | Propagar output config (directory, interval, formats)      |
| **Modificar** | `tools/molab_web_gui.py`          | Eliminar datos fake, no hardcodear initial_state           |

## 4. Riesgos y Mitigación

| Riesgo                                               | Mitigación                                                    |
|------------------------------------------------------|---------------------------------------------------------------|
| Doble gravedad con plugin environment                | Documentar: si el plugin environment está activo, desactivar gravedad del motor (`enable_gravity = false`) |
| Inestabilidad numérica al cambiar integrador         | Validar estado cada 50 ticks (ya existe)                      |
| Ruptura de configuraciones JSON existentes           | Usar `value()` con defaults para nuevos campos                |
| Output directory no existe                           | Crear directorio automáticamente si no existe                 |

## 5. Testing Manual

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
