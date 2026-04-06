# Configuration reference

This document defines the expected behavior of each configuration variable in
`data/defaults/default_config.json`.

## Root structure

- `simulation`: global simulation execution controls.
- `plugins`: plugin loading and per-plugin parameters.
- `initial_state_file`: initial state file.
- `output_directory`: base result directory.
- `logging`: output behavior and log retention.
- `performance`: performance instrumentation and execution tuning.
- `validation`: numeric and state-sanity validations.
- `vehicle_models`: vehicle data sources (aero, geometry, propulsion, mass, limits).
- `mission_environment`: planet, flight sequence, atmosphere/wind, integrator.

---

## `simulation`

- `simulation.duration` (`number`, seconds)  
  Maximum simulated time before normal stop condition.

- `simulation.max_iterations` (`integer`)  
  Maximum number of ticks before forced stop.

- `simulation.enable_logging` (`boolean`)  
  Master switch for simulation log emission.

- `simulation.log_file` (`string`, path)  
  Path to persisted log file.

- `simulation.log_level` (`string`)  
  Minimum severity level to emit (for example: `DEBUG`, `INFO`, `WARNING`, `ERROR`).

---

## `plugins[]`

Each `plugins` entry configures one plugin.

- `plugins[].name` (`string`)  
  Logical plugin name for log/UI identification.

- `plugins[].type` (`integer`)  
  Plugin execution role:
  - `0`: sequential state modifier (mutates state directly)
  - `1`: parallel physics calculator (force/torque output)

- `plugins[].library_path` (`string`)  
  Shared-library path/name to load plugin.

- `plugins[].enabled` (`boolean`)  
  Enables/disables plugin load and execution.

- `plugins[].parameters` (`object`)  
  Free-form plugin configuration passed to `plugin_configure` API.

- `plugins[].parameters.use_host_logger` (`boolean`, optional, default `false`)  
  If `true`, `PluginManager` attempts to inject host logging services via
  `plugin_set_host_services(const PluginHostServices*)`.

Example parameters in default file:
- `plugins[].parameters.step_size` (`number`)  
  Plugin-defined step factor.
- `plugins[].parameters.debug_output` (`boolean`)  
  Plugin-defined debug output activation.

---

## Global input/output

- `initial_state_file` (`string`, path)  
  Source file with initial simulation state.

- `output_directory` (`string`, path)  
  Root directory for artifacts/results.

---

## `logging`

- `logging.console_output` (`boolean`)  
  If `true`, write logs to console/stdout.

- `logging.file_output` (`boolean`)  
  If `true`, write logs to file.

- `logging.log_rotation` (`boolean`)  
  Enable log rotation when size threshold is reached.

- `logging.max_file_size_mb` (`number`, MB)  
  Maximum file size before rotation.

- `logging.max_files` (`integer`)  
  Number of rotated files to keep.

---

## `performance`

- `performance.enable_metrics` (`boolean`)  
  Enables metrics collection (tick time, plugin timings, etc.).

- `performance.metrics_output_interval` (`integer`)  
  Metrics publication interval (typically every N ticks).

- `performance.enable_profiling` (`boolean`)  
  Enables deeper profiling/tracing with higher overhead.

- `performance.thread_pool_size` (`integer`)  
  Number of workers for parallelizable tasks.

---

## `validation`

- `validation.max_position_magnitude` (`number`)  
  Upper position magnitude limit before warning/error policy is triggered.

- `validation.max_velocity_magnitude` (`number`)  
  Upper velocity magnitude limit before warning/error policy is triggered.

- `validation.enable_nan_checks` (`boolean`)  
  Enables NaN/Inf checks on simulation state.

---

## `vehicle_models`

### `vehicle_models.aero_database`
- `type` (`string`)  
  Aerodynamic model type (for example `lookup_table`).
- `source.uri` (`string`, path/URI)  
  Aerodynamic data source location.
- `source.format` (`string`)  
  Data format (for example `hdf5`).
- `axes` (`array<string>`)  
  Independent variables for lookup/interpolation (for example `alpha_deg`, `beta_deg`, `mach`).

### `vehicle_models.reference_geometry`
- `source.uri` (`string`, path/URI)  
  Geometry dataset location.
- `source.format` (`string`)  
  Geometry dataset format (for example `csv`).

### `vehicle_models.propulsion_model`
- `source.uri` (`string`, path/URI)  
  Propulsion performance dataset location.
- `source.format` (`string`)  
  Propulsion dataset format (for example `csv`).

### `vehicle_models.mass_properties`
- `source.uri` (`string`, path/URI)  
  Mass/inertia dataset location.
- `source.format` (`string`)  
  Mass properties format (for example `json`).

### `vehicle_models.physical_limits`
- `actuators[]` (`array<object>`)  
  Per-actuator constraints.
  - `actuators[].id` (`string`)  
    Actuator identifier.
  - `actuators[].max_rate_deg_s` (`number`, deg/s)  
    Maximum deflection rate.
  - `actuators[].range_deg` (`number`, deg)  
    Maximum absolute deflection range.
- `structural.source.uri` (`string`, path/URI)  
  Structural constraints dataset location.
- `structural.source.format` (`string`)  
  Structural limits format (for example `csv`).

---

## `mission_environment`

### `mission_environment.planet_model`
- `planet_model` (`string`)  
  Planetary/geodetic model selection (for example `WGS84`).

### `mission_environment.flight_sequence`
- `source.uri` (`string`, path/URI)  
  Mission sequence/phases definition file.
- `source.format` (`string`)  
  Sequence format (for example `yaml`).

### `mission_environment.atmosphere_wind`
- `model` (`string`)  
  Atmospheric model selection (for example `US_Standard_1976`).
- `wind_profile_source.uri` (`string`, path/URI)  
  Wind profile dataset location.
- `wind_profile_source.format` (`string`)  
  Wind profile format (for example `csv`).

### `mission_environment.integrator_config`
- `method` (`string`)  
  Numerical integration method (for example `runge_kutta_4`).
- `rtol` (`number`)  
  Relative tolerance for adaptive control/error.
- `atol` (`number`)  
  Absolute tolerance for adaptive control/error.

---

## Notes

- Paths may be relative to process working directory or repository root, depending on launcher/runtime.
- Units should be treated as mandatory contract where specified (seconds, deg/s, MB, etc.).
- Keys in `plugins[].parameters` belong to plugin implementation and may vary.
- `use_host_logger` is a host-reserved/interpreted key to enable centralized host logging integration in plugins.

---

Documentation status: **Current**

Back to: [`docs/README.md`](../README.md)
