# Referencia de configuración

Este documento define el comportamiento esperado de cada variable de configuración en
`data/defaults/default_config.json`.

## Estructura raíz

- `simulation`: controles globales de ejecución de la simulación.
- `plugins`: carga de plugins y parámetros específicos por plugin.
- `initial_state_file`: archivo de estado inicial.
- `output_directory`: directorio base de resultados.
- `logging`: comportamiento de salida y retención de logs.
- `performance`: instrumentación de rendimiento y ajuste de ejecución.
- `validation`: validaciones numéricas y de sanidad de estado.
- `vehicle_models`: fuentes de datos del vehículo (aero, geometría, propulsión, masa y límites).
- `mission_environment`: planeta, secuencia de vuelo, atmósfera/viento e integrador.

---

## `simulation`

- `simulation.duration` (`number`, segundos)  
  Tiempo simulado máximo antes de condición normal de parada.

- `simulation.max_iterations` (`integer`)  
  Número máximo de ticks antes de parada forzada.

- `simulation.enable_logging` (`boolean`)  
  Interruptor maestro para emisión de logs de simulación.

- `simulation.log_file` (`string`, ruta)  
  Ruta del archivo de logs persistidos.

- `simulation.log_level` (`string`)  
  Nivel mínimo de severidad a emitir (por ejemplo: `DEBUG`, `INFO`, `WARNING`, `ERROR`).

---

## `plugins[]`

Cada entrada de `plugins` configura un plugin.

- `plugins[].name` (`string`)  
  Nombre lógico del plugin para identificación en logs/UI.

- `plugins[].type` (`integer`)  
  Rol de ejecución del plugin:
  - `0`: modificador secuencial de estado (muta estado directamente)
  - `1`: calculador físico paralelo (salida de fuerza/torque)

- `plugins[].library_path` (`string`)  
  Ruta/nombre de la biblioteca compartida para cargar el plugin.

- `plugins[].enabled` (`boolean`)  
  Habilita/deshabilita carga y ejecución del plugin.

- `plugins[].parameters` (`object`)  
  Configuración libre del plugin, pasada al API `plugin_configure`.

Ejemplo de parámetros en el archivo por defecto:
- `plugins[].parameters.step_size` (`number`)  
  Factor de paso definido por el plugin.
- `plugins[].parameters.debug_output` (`boolean`)  
  Activación de salida de depuración definida por el plugin.

---

## Entrada/Salida global

- `initial_state_file` (`string`, ruta)  
  Archivo fuente con el estado inicial de simulación.

- `output_directory` (`string`, ruta)  
  Directorio raíz de artefactos/resultados.

---

## `logging`

- `logging.console_output` (`boolean`)  
  Si es `true`, escribe logs en consola/stdout.

- `logging.file_output` (`boolean`)  
  Si es `true`, escribe logs en archivo.

- `logging.log_rotation` (`boolean`)  
  Activa rotación de logs al alcanzar límite de tamaño.

- `logging.max_file_size_mb` (`number`, MB)  
  Tamaño máximo de archivo antes de rotar.

- `logging.max_files` (`integer`)  
  Cantidad de archivos rotados a conservar.

---

## `performance`

- `performance.enable_metrics` (`boolean`)  
  Activa recolección de métricas (tiempo de tick, tiempos de plugins, etc.).

- `performance.metrics_output_interval` (`integer`)  
  Intervalo de publicación de métricas (típicamente cada N ticks).

- `performance.enable_profiling` (`boolean`)  
  Activa profiling/tracing profundo con mayor overhead.

- `performance.thread_pool_size` (`integer`)  
  Número de workers para tareas paralelizables.

---

## `validation`

- `validation.max_position_magnitude` (`number`)  
  Límite superior de magnitud de posición antes de activar política de warning/error.

- `validation.max_velocity_magnitude` (`number`)  
  Límite superior de magnitud de velocidad antes de activar política de warning/error.

- `validation.enable_nan_checks` (`boolean`)  
  Activa validaciones de NaN/Inf en el estado de simulación.

---

## `vehicle_models`

### `vehicle_models.aero_database`
- `type` (`string`)  
  Tipo de modelo aerodinámico (por ejemplo `lookup_table`).
- `source.uri` (`string`, ruta/URI)  
  Ubicación de la fuente de datos aerodinámicos.
- `source.format` (`string`)  
  Formato de datos (por ejemplo `hdf5`).
- `axes` (`array<string>`)  
  Variables independientes para lookup/interpolación (por ejemplo `alpha_deg`, `beta_deg`, `mach`).

### `vehicle_models.reference_geometry`
- `source.uri` (`string`, ruta/URI)  
  Ubicación del dataset de geometría.
- `source.format` (`string`)  
  Formato del dataset de geometría (por ejemplo `csv`).

### `vehicle_models.propulsion_model`
- `source.uri` (`string`, ruta/URI)  
  Ubicación del dataset de rendimiento de propulsión.
- `source.format` (`string`)  
  Formato del dataset de propulsión (por ejemplo `csv`).

### `vehicle_models.mass_properties`
- `source.uri` (`string`, ruta/URI)  
  Ubicación del dataset de masa/inercia.
- `source.format` (`string`)  
  Formato de propiedades de masa (por ejemplo `json`).

### `vehicle_models.physical_limits`
- `actuators[]` (`array<object>`)  
  Restricciones por actuador.
  - `actuators[].id` (`string`)  
    Identificador de actuador.
  - `actuators[].max_rate_deg_s` (`number`, deg/s)  
    Tasa máxima de deflexión.
  - `actuators[].range_deg` (`number`, deg)  
    Rango máximo absoluto de deflexión.
- `structural.source.uri` (`string`, ruta/URI)  
  Ubicación del dataset de restricciones estructurales.
- `structural.source.format` (`string`)  
  Formato de límites estructurales (por ejemplo `csv`).

---

## `mission_environment`

### `mission_environment.planet_model`
- `planet_model` (`string`)  
  Selección de modelo planetario/geodésico (por ejemplo `WGS84`).

### `mission_environment.flight_sequence`
- `source.uri` (`string`, ruta/URI)  
  Archivo de definición de secuencia/fases de misión.
- `source.format` (`string`)  
  Formato de la secuencia (por ejemplo `yaml`).

### `mission_environment.atmosphere_wind`
- `model` (`string`)  
  Selección de modelo atmosférico (por ejemplo `US_Standard_1976`).
- `wind_profile_source.uri` (`string`, ruta/URI)  
  Ubicación del dataset de perfil de viento.
- `wind_profile_source.format` (`string`)  
  Formato del perfil de viento (por ejemplo `csv`).

### `mission_environment.integrator_config`
- `method` (`string`)  
  Método de integración numérica (por ejemplo `runge_kutta_4`).
- `rtol` (`number`)  
  Tolerancia relativa para control adaptativo/error.
- `atol` (`number`)  
  Tolerancia absoluta para control adaptativo/error.

---

## Notas

- Las rutas pueden ser relativas al directorio de trabajo del proceso o a la raíz del repositorio, según el launcher/runtime.
- Las unidades deben tratarse como contrato obligatorio cuando están especificadas (segundos, deg/s, MB, etc.).
- Las claves en `plugins[].parameters` son propiedad del plugin y pueden variar según implementación.

---

Estado de documentación: **Current**

Volver a: [`docs/README.md`](../README.md)
