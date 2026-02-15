# Especificación Funcional — Validación de Configuración del Simulador

**Rama:** `feature/validacion-configuracion-simulador`  
**Fecha:** 2026-02-12  
**Estado:** Borrador — Pendiente de revisión

---

## 1. Objetivo

Garantizar que los parámetros de simulación configurados desde la interfaz web se apliquen correctamente en el motor de simulación C++, produciendo resultados que varíen de forma realista según la configuración. Actualmente, el simulador produce los mismos valores independientemente de los cambios realizados en la web.

## 2. Contexto y Problema Actual

El simulador MoLab permite configurar parámetros de física desde la interfaz web (`molab_web_gui.py`): gravedad, drag atmosférico, tipo de integrador, duración, time step, intervalo de salida, etc. Sin embargo, al ejecutar simulaciones con distintas configuraciones, **los resultados CSV son esencialmente idénticos**. Esto se debe a múltiples desconexiones entre la configuración web y el motor C++.

### Problemas detectados

| #  | Problema                                           | Ubicación                                          | Impacto                                      |
|----|----------------------------------------------------|----------------------------------------------------|----------------------------------------------|
| P1 | Gravedad hardcodeada como `-9.81`                  | `PluginManager.cpp:309`                            | Ignora `enable_gravity` y `gravity_magnitude` |
| P2 | `gravity_magnitude` no existe en C++               | `ConfigManager.h` (PhysicsConfig)                  | El valor de la web se pierde                  |
| P3 | Tipo de integrador nunca se aplica                 | `PluginManager()` constructor                      | Siempre usa RK4, ignora config               |
| P4 | Condiciones atmosféricas estáticas en state buffer | `PluginManager.cpp:340-346`                        | Densidad/presión/temperatura nunca cambian    |
| P5 | `enable_atmospheric_drag` nunca se usa             | Motor de física (`apply_physics_integration`)       | Drag solo funciona si hay plugin cargado      |
| P6 | `output_directory` hardcodeado                     | `SimulationEngine.cpp:113`                         | Ignora directorio configurado en la web       |
| P7 | `output_interval` hardcodeado a 5                  | `SimulationEngine.cpp:115`                         | Ignora intervalo configurado en la web        |
| P8 | Estado inicial siempre el mismo                    | `molab_web_gui.py:2746`                            | Hardcodea `realistic_state.json`              |
| P9 | Progreso del frontend es falso                     | `molab_web_gui.py` JS `monitorSimulationProgress`  | Muestra posición/velocidad simulada fake      |
| P10| Doble gravedad cuando el plugin environment está activo | `PluginManager.cpp:309` + `environment.cpp:432` | Motor suma gravedad hardcoded + plugin gravity |

## 3. Usuarios Objetivo

- Equipo de desarrollo de MoLab.
- Ingenieros que necesitan validar que el simulador responde correctamente a los parámetros configurados.

## 4. Requisitos Funcionales

### RF-01: Gravedad configurable

- Si `enable_gravity = false`, no se debe aplicar fuerza gravitacional desde el motor (los plugins pueden agregar su propia gravedad).
- Si `enable_gravity = true`, usar el valor de `gravity_magnitude` configurado en la web (no hardcodear 9.81).
- `gravity_magnitude` debe propagarse desde la web → JSON config → C++ `PhysicsConfig`.

### RF-02: Tipo de integrador configurable

- El integrador (`euler`, `runge_kutta_4`, `verlet`) seleccionado en la web debe aplicarse al `PhysicsIntegrator` del motor.
- Cambiar el integrador debe producir resultados numéricos diferentes (Euler vs RK4 difieren en precisión).

### RF-03: Condiciones atmosféricas dinámicas

- Densidad, presión y temperatura atmosférica deben actualizarse cada tick basándose en la altitud actual del vehículo.
- Si `enable_atmospheric_drag = true`, el motor debe aplicar drag incluso sin plugins de aerodinámica cargados.

### RF-04: Parámetros de salida configurables

- `output_directory` configurado en la web debe respetarse.
- `output_interval` configurado en la web debe respetarse.

### RF-05: Eliminación de duplicación de fuerzas

- Cuando un plugin (ej. `environment`) ya calcula gravedad, el motor no debe agregar gravedad adicional hardcodeada.
- Definir claramente la responsabilidad: o el motor aplica gravedad, o los plugins lo hacen, nunca ambos.

### RF-06: Progreso real en el frontend

- El frontend debe mostrar datos reales de la simulación (posición, velocidad) obtenidos del backend, no valores simulados con fórmulas JavaScript fake.

### RF-07: Limpieza de código muerto

- Eliminar funciones, variables y secciones de código que no contribuyen al flujo de simulación.
- Consolidar código duplicado (ej. modelos atmosféricos duplicados en `PhysicsIntegrator.cpp` y `environment.cpp`).

## 5. Requisitos No Funcionales

| ID     | Requisito              | Detalle                                                                   |
|--------|------------------------|---------------------------------------------------------------------------|
| RNF-01 | **Retrocompatibilidad** | Las configuraciones JSON existentes deben seguir funcionando.            |
| RNF-02 | **Rendimiento**        | No degradar el rendimiento actual del simulador (< 10% overhead).        |
| RNF-03 | **Estabilidad**        | No introducir inestabilidades numéricas (NaN, overflow).                 |
| RNF-04 | **Compilación**        | El proyecto debe compilar sin errores ni warnings en Windows (MSVC).     |

## 6. Criterios de Aceptación

1. Cambiar `enable_gravity` a `false` produce una trayectoria sin caída gravitacional.
2. Cambiar `gravity_magnitude` a un valor diferente (ej. 3.71 para Marte) produce resultados distintos.
3. Cambiar el integrador de `runge_kutta_4` a `euler` produce diferencias numéricas detectables.
4. Las condiciones atmosféricas en el CSV cambian con la altitud del vehículo.
5. `output_interval` configurado en la web se refleja en el CSV generado.
6. No hay duplicación de gravedad cuando se usan plugins de environment.
7. El frontend muestra datos reales de la simulación después de completarse.

## 7. Entregables

| # | Entregable                                          | Ubicación                          |
|---|-----------------------------------------------------|------------------------------------|
| 1 | Corrección del motor de física                      | `core/PluginManager.cpp`           |
| 2 | Extensión de `PhysicsConfig` con `gravity_magnitude` | `src/core/ConfigManager.h/.cpp`   |
| 3 | Aplicación del integrador desde config              | `core/PluginManager.cpp`           |
| 4 | Condiciones atmosféricas dinámicas                  | `core/PluginManager.cpp`           |
| 5 | Propagación de output config                        | `core/SimulationEngine.cpp`        |
| 6 | Limpieza de código muerto/duplicado                 | Múltiples archivos                 |

## 8. Fuera de Alcance

- Nuevos modelos físicos (propulsión, estructuras).
- Cambios en la arquitectura de plugins.
- Nuevos tipos de integrador.
- Modificaciones al esquema FlatBuffer del state vector.
- Cambios en la interfaz web (excepto corrección del progreso fake).
