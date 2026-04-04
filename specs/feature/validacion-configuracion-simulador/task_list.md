# Lista de Tareas — Validación de Configuración del Simulador

**Rama:** `feature/validacion-configuracion-simulador`  
**Fecha:** 2026-02-12  
**Referencia:** [Spec Funcional](./functional_spec.md) | [Spec Técnica](./technical_spec.md)

---

## Tareas

### T-01: Extender `PhysicsConfig` con `gravity_magnitude`

- **Prioridad:** Alta (bloqueante para T-02)
- **Descripción:** Agregar campo `double gravity_magnitude` a `PhysicsConfig` en `ConfigManager.h`. Actualizar `parsePhysicsConfig()`, `setDefaults()` y `saveConfig()` en `ConfigManager.cpp` para leer/escribir este campo con default `9.81`.
- **Archivos:** `src/core/ConfigManager.h`, `src/core/ConfigManager.cpp`
- **Criterio de completitud:** El campo se parsea correctamente desde el JSON y se puede leer con `getPhysicsConfig().gravity_magnitude`.
- **Ref:** Spec Técnica §2.1.1

---

### T-02: Corregir gravedad en `apply_physics_integration()`

- **Prioridad:** Alta
- **Dependencia:** T-01
- **Descripción:** En `PluginManager.cpp`, reemplazar la gravedad hardcodeada (`-9.81`) por la lectura de `PhysicsConfig`. Si `enable_gravity = false`, no agregar fuerza gravitacional. Si `enable_gravity = true`, usar `gravity_magnitude` del config.
- **Archivos:** `core/PluginManager.cpp`
- **Criterio de completitud:** Desactivar gravedad en la web produce trayectoria sin caída. Cambiar `gravity_magnitude` produce resultados distintos.
- **Ref:** Spec Técnica §2.1.2

---

### T-03: Aplicar tipo de integrador desde config

- **Prioridad:** Alta
- **Descripción:** Configurar el `PhysicsIntegrator` con el tipo definido en `PhysicsConfig::integrator_type` al inicio de la simulación (en `load_plugins_from_config()` o al crear el `PluginManager`).
- **Archivos:** `core/PluginManager.cpp`
- **Criterio de completitud:** Cambiar integrador en la web (euler vs runge_kutta_4) produce diferencias numéricas en el CSV.
- **Ref:** Spec Técnica §2.1.3

---

### T-04: Actualizar condiciones atmosféricas dinámicamente

- **Prioridad:** Alta
- **Descripción:** En `apply_physics_integration()`, después de integrar la nueva posición, recalcular `atm_density`, `atm_pressure` y `atm_temperature` basándose en la altitud actual usando `AtmosphericEffects::calculateAirDensity()` (ya existe). Usar estos valores al crear el nuevo `GeneralState` FlatBuffer.
- **Archivos:** `core/PluginManager.cpp`
- **Criterio de completitud:** En el CSV de salida, `atm_density`, `atm_pressure` y `atm_temperature` cambian cuando la altitud cambia.
- **Ref:** Spec Técnica §2.1.4

---

### T-05: Aplicar drag atmosférico desde config del motor

- **Prioridad:** Media
- **Dependencia:** T-04
- **Descripción:** Si `PhysicsConfig::enable_atmospheric_drag = true`, calcular y aplicar fuerza de drag en `apply_physics_integration()` usando `AtmosphericEffects::calculateDrag()`. Esto actúa como drag básico del motor, independiente de los plugins de aerodinámica.
- **Archivos:** `core/PluginManager.cpp`
- **Criterio de completitud:** Activar drag en la web (sin plugins de aerodinámica) produce desaceleración visible en el CSV.
- **Ref:** Spec Técnica §2.1.5

---

### T-06: Propagar configuración de output desde config

- **Prioridad:** Alta
- **Descripción:** En `SimulationEngine::initialize_with_config()`, leer `output_directory`, `output_interval` y formatos de output desde el JSON de configuración en lugar de hardcodearlos. Agregar parseo de sección `output` en `ConfigManager` o leer directamente del JSON.
- **Archivos:** `core/SimulationEngine.cpp`, opcionalmente `src/core/ConfigManager.h/.cpp`
- **Criterio de completitud:** Configurar `output_interval = 10` en la web produce un CSV con datos cada 10 ticks.
- **Ref:** Spec Técnica §2.1.6

---

### T-07: No hardcodear `initial_state_file` en la web

- **Prioridad:** Media
- **Descripción:** En `molab_web_gui.py`, usar el `initial_state_file` que el `ConfigManager` ya maneja con un default sensato (`data/initial_state.json`), en lugar de hardcodear `realistic_state.json`.
- **Archivos:** `tools/molab_web_gui.py`
- **Criterio de completitud:** El simulador usa el estado inicial correcto definido en la configuración.
- **Ref:** Spec Técnica §2.2.1

---

### T-08: Eliminar datos fake del frontend

- **Prioridad:** Media
- **Descripción:** Eliminar las fórmulas JavaScript en `monitorSimulationProgress()` que simulan posición y velocidad fake. Dejar solo la barra de progreso basada en tiempo estimado.
- **Archivos:** `tools/molab_web_gui.py` (JS embebido)
- **Criterio de completitud:** El frontend no muestra posición/velocidad fake durante la simulación. Solo muestra barra de progreso y tick actual estimado.
- **Ref:** Spec Técnica §2.2.2

---

### T-09: Limpieza de código duplicado

- **Prioridad:** Baja
- **Descripción:** Revisar y consolidar:
  - Modelo atmosférico duplicado entre `PhysicsIntegrator.cpp` (líneas 274-325) y `environment.cpp`.
  - Verificar que no haya funciones no utilizadas en el flujo de simulación.
- **Archivos:** `src/core/PhysicsIntegrator.cpp`, revisión general
- **Criterio de completitud:** No hay funciones duplicadas que hagan lo mismo. Código muerto identificado y documentado.
- **Ref:** Spec Técnica §2.3

---

### T-10: Recompilar y verificar end-to-end

- **Prioridad:** Alta
- **Dependencia:** T-01 a T-08
- **Descripción:** Recompilar el proyecto y ejecutar las pruebas manuales definidas en la spec técnica:
  - [ ] Gravedad desactivada → trayectoria sin caída.
  - [ ] Gravedad de Marte (3.71) → caída más lenta.
  - [ ] Cambio de integrador → diferencias numéricas.
  - [ ] Condiciones atmosféricas dinámicas en el CSV.
  - [ ] Output interval configurable.
  - [ ] Sin datos fake en el frontend.
- **Criterio de completitud:** Todos los tests pasan.
- **Ref:** Spec Técnica §5

---

## Resumen

| Tarea | Descripción                                    | Prioridad | Dependencia |
|-------|------------------------------------------------|-----------|-------------|
| T-01  | Agregar `gravity_magnitude` a PhysicsConfig    | Alta      | —           |
| T-02  | Corregir gravedad hardcodeada                  | Alta      | T-01        |
| T-03  | Aplicar integrador desde config                | Alta      | —           |
| T-04  | Condiciones atmosféricas dinámicas             | Alta      | —           |
| T-05  | Drag atmosférico desde config del motor        | Media     | T-04        |
| T-06  | Propagar output config                         | Alta      | —           |
| T-07  | No hardcodear initial_state en la web          | Media     | —           |
| T-08  | Eliminar datos fake del frontend               | Media     | —           |
| T-09  | Limpieza de código duplicado                   | Baja      | —           |
| T-10  | Recompilar y verificar end-to-end              | Alta      | T-01–T-08   |

**Total: 10 tareas**  
**Ruta crítica:** T-01 → T-02 → T-10 (verificación)  
**Paralelas:** T-03, T-04, T-06, T-07, T-08 pueden hacerse en paralelo
