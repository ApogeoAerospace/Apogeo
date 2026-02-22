# Especificación Funcional — Validación de Configuración del Simulador

**Rama:** `feature/validacion-configuracion-simulador`  
**Fecha:** 2026-02-22 (Actualizado)  
**Estado:** En Revisión

---

## 1. Objetivo

Implementar un sistema robusto de validación de configuración que garantice:
1. Todos los parámetros de configuración JSON sean válidos antes de iniciar la simulación
2. Los valores configurados se propaguen correctamente desde el archivo JSON hasta el motor C++
3. El sistema detecte y reporte configuraciones inválidas o inconsistentes
4. Se mantenga compatibilidad con configuraciones existentes mientras se soporta la nueva estructura modular

## 2. Contexto y Problema Actual

### Estructura Actual de Configuración

MoLab utiliza un archivo JSON modular con las siguientes secciones:

```json
{
  "simulation": { /* Parámetros básicos de simulación */ },
  "plugins": [ /* Lista de plugins con parámetros */ ],
  "logging": { /* Configuración de logging */ },
  "performance": { /* Métricas y profiling */ },
  "validation": { /* Límites de validación */ },
  "vehicle_models": { /* Modelos aeroespaciales */ },
  "mission_environment": { /* Ambiente y planeta */ },
  "initial_state_file": "path/to/state.json",
  "output_directory": "output/"
}
```

### Problemas Identificados

| #  | Problema                                           | Impacto                                      |
|----|----------------------------------------------------|-------------------------------------------------|
| P1 | No hay validación de tipos de datos               | Valores inválidos causan crashes en runtime     |
| P2 | No hay validación de rangos numéricos             | Valores extremos causan inestabilidad numérica  |
| P3 | Rutas de archivos no se validan antes de usar     | Errores tardíos durante ejecución               |
| P4 | Plugins con library_path específico de plataforma | Configuraciones no portables entre OS           |
| P5 | No hay validación de dependencias entre secciones | Configuraciones inconsistentes                  |
| P6 | Parámetros de plugins no se validan               | Plugins reciben datos inválidos                 |
| P7 | No hay esquema JSON formal                        | Difícil detectar errores de configuración       |
| P8 | Valores por defecto no están documentados         | Comportamiento impredecible con configs parciales|

## 3. Usuarios Objetivo

- Equipo de desarrollo de MoLab.
- Ingenieros que necesitan validar que el simulador responde correctamente a los parámetros configurados.

## 4. Requisitos Funcionales

### RF-01: Validación de Estructura JSON

- Validar que todas las secciones requeridas estén presentes: `simulation`, `plugins`
- Validar que las secciones opcionales tengan la estructura correcta si están presentes
- Reportar errores específicos indicando qué campo falta o es inválido
- Soportar configuraciones parciales con valores por defecto documentados

### RF-02: Validación de Tipos de Datos

**Sección `simulation`:**
- `duration`: float > 0
- `max_iterations`: int > 0
- `enable_logging`: boolean
- `log_file`: string (ruta válida)
- `log_level`: enum ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]

**Sección `plugins`:**
- `name`: string no vacío
- `type`: int [0, 1] (SEQUENTIAL_STATE_MODIFIER, PARALLEL_PHYSICS_CALCULATOR)
- `library_path`: string (sin extensión específica de plataforma)
- `enabled`: boolean
- `parameters`: object (validación específica por plugin)

**Sección `mission_environment.integrator_config`:**
- `method`: enum ["euler", "runge_kutta_4", "verlet"]
- `rtol`: float > 0
- `atol`: float > 0

### RF-03: Validación de Rangos Numéricos

- `simulation.duration`: [0.001, 86400.0] (1ms a 24 horas)
- `simulation.max_iterations`: [1, 1000000]
- `performance.thread_pool_size`: [1, 64]
- `validation.max_position_magnitude`: [1.0, 1e9]
- `validation.max_velocity_magnitude`: [1.0, 100000.0]
- `mission_environment.integrator_config.rtol`: [1e-12, 1e-3]
- `mission_environment.integrator_config.atol`: [1e-15, 1e-6]

### RF-04: Validación de Rutas de Archivos

- `initial_state_file`: debe existir o ser ruta válida para creación
- `output_directory`: debe ser directorio escribible o crearse automáticamente
- `logging.log_file`: directorio padre debe existir
- `vehicle_models.*.source.uri`: validar existencia si formato != "none"

### RF-05: Validación de Dependencias

- Si `logging.file_output = true`, `logging.log_file` debe estar especificado
- Si `performance.enable_profiling = true`, `performance.enable_metrics` debe ser true
- Si un plugin está `enabled = true`, su `library_path` debe ser válido
- Si `vehicle_models` especifica archivos, validar formato coincide con extensión

### RF-06: Validación de Parámetros de Plugins

**Aerodynamics Plugin:**
- `reference_area`: float > 0
- `drag_coefficient`: float >= 0
- `enable_drag`: boolean

**Propulsion Plugin:**
- `sea_level_thrust`: float >= 0
- `specific_impulse_sl`: float > 0
- `engine_on`: boolean

**Environment Plugin:**
- `enable_atmospheric_model`: boolean
- `enable_wind_effects`: boolean

**Structures Plugin:**
- `enable_mass_tracking`: boolean
- `enable_inertia_calculation`: boolean

### RF-07: Normalización de Rutas Multiplataforma

- Convertir `library_path` sin extensión a extensión correcta (.dll/.dylib/.so)
- Normalizar separadores de ruta (/ vs \) según plataforma
- Resolver rutas relativas desde directorio de configuración

### RF-08: Reportes de Validación

- Generar reporte detallado de validación con:
  - Lista de errores críticos (bloquean ejecución)
  - Lista de warnings (configuración subóptima)
  - Lista de valores por defecto aplicados
  - Resumen de configuración validada
- Formato de salida: consola (coloreado) y archivo log

## 5. Requisitos No Funcionales

| ID     | Requisito              | Detalle                                                                   |
|--------|------------------------|---------------------------------------------------------------------------|
| RNF-01 | **Retrocompatibilidad** | Las configuraciones JSON existentes deben seguir funcionando.            |
| RNF-02 | **Rendimiento**        | No degradar el rendimiento actual del simulador (< 10% overhead).        |
| RNF-03 | **Estabilidad**        | No introducir inestabilidades numéricas (NaN, overflow).                 |
| RNF-04 | **Compilación**        | El proyecto debe compilar sin errores ni warnings en Windows (MSVC).     |

## 6. Criterios de Aceptación

### Validación Básica
1. Configuración con campo `simulation.duration` negativo es rechazada con error específico
2. Configuración con `log_level` inválido (ej. "TRACE") es rechazada
3. Configuración con `initial_state_file` inexistente genera warning pero continúa con defaults
4. Configuración sin sección `simulation` es rechazada

### Validación de Plugins
5. Plugin con `library_path` usando `.dylib` se normaliza automáticamente según plataforma
6. Plugin con `type` fuera de rango [0,1] es rechazado
7. Plugin con parámetros inválidos genera error específico del plugin

### Validación de Rangos
8. `simulation.duration` = 0.0001 (100μs) es rechazado (< mínimo 1ms)
9. `thread_pool_size` = 100 es rechazado (> máximo 64)
10. `rtol` = 1.0 es rechazado (> máximo 1e-3)

### Validación de Dependencias
11. `file_output=true` sin `log_file` genera error
12. `enable_profiling=true` sin `enable_metrics=true` genera error

### Reportes
13. Configuración válida genera reporte con "0 errors, 0 warnings"
14. Configuración con warnings permite ejecución pero reporta issues
15. Configuración con errors bloquea ejecución y muestra lista de problemas

### Retrocompatibilidad
16. Configuraciones antiguas sin sección `validation` funcionan con defaults
17. Configuraciones sin `mission_environment` funcionan con defaults
18. Plugins sin `parameters` funcionan (objeto vacío por defecto)

## 7. Entregables

| # | Entregable                                          | Ubicación                          |
|---|-----------------------------------------------------|------------------------------------|
| 1 | Sistema de validación de configuración              | `src/core/ConfigValidator.h/.cpp`  |
| 2 | Extensión de ConfigManager con validación           | `src/core/ConfigManager.h/.cpp`    |
| 3 | Esquema JSON de validación                          | `data/schemas/config_schema.json`  |
| 4 | Validadores específicos de plugins                  | `src/core/PluginValidator.h/.cpp`  |
| 5 | Reportes de validación                              | `src/core/ValidationReport.h/.cpp` |
| 6 | Tests unitarios de validación                       | `tests/config_validation_test.cpp` |
| 7 | Documentación de configuración                      | `docs/configuration_guide.md`      |
| 8 | Ejemplos de configuraciones válidas/inválidas       | `data/examples/configs/`           |

## 8. Fuera de Alcance

- Validación en tiempo de ejecución (solo pre-ejecución)
- Validación de datos del state buffer (FlatBuffers)
- Corrección automática de configuraciones inválidas
- Migración automática de configuraciones antiguas
- Validación de contenido de archivos externos (aero databases, etc.)
- Interfaz gráfica para edición de configuración
- Validación de coherencia física (ej. thrust > weight)
- Generación automática de configuraciones óptimas

## 9. Estructura de Configuración Completa

### Secciones Requeridas

```json
{
  "simulation": {
    "duration": 100.0,           // float, segundos
    "max_iterations": 10,        // int, límite de ticks
    "enable_logging": true,      // boolean
    "log_file": "logs/molab.log",// string
    "log_level": "INFO"          // enum
  },
  "plugins": [                   // array
    {
      "name": "example_plugin",  // string
      "type": 0,                 // int [0,1]
      "library_path": "build/lib/libexample_plugin", // sin extensión
      "enabled": true,           // boolean
      "parameters": {}           // object (específico por plugin)
    }
  ]
}
```

### Secciones Opcionales

```json
{
  "logging": {
    "console_output": true,
    "file_output": true,
    "log_rotation": true,
    "max_file_size_mb": 100,
    "max_files": 5
  },
  "performance": {
    "enable_metrics": true,
    "metrics_output_interval": 1000,
    "enable_profiling": false,
    "thread_pool_size": 4
  },
  "validation": {
    "max_position_magnitude": 1000000.0,
    "max_velocity_magnitude": 10000.0,
    "enable_nan_checks": true
  },
  "vehicle_models": {
    "aero_database": { /* ... */ },
    "reference_geometry": { /* ... */ },
    "propulsion_model": { /* ... */ },
    "mass_properties": { /* ... */ }
  },
  "mission_environment": {
    "planet_model": "WGS84",
    "atmosphere_wind": {
      "model": "US_Standard_1976"
    },
    "integrator_config": {
      "method": "runge_kutta_4",
      "rtol": 1e-6,
      "atol": 1e-8
    }
  },
  "initial_state_file": "data/defaults/default_state.json",
  "output_directory": "output/"
}
```
