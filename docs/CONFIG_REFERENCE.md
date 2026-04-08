# Configuration Reference

This document defines the intended behavior of each configuration variable in
`data/defaults/default_config.json`.

Configuration is loaded once during bootstrap in `main.cpp` and then consumed by runtime components through `ConfigManager`.
`ConfigManager` remains silent until load success. If loading fails, it forces full logging and emits detailed diagnostics.
Parse/validation errors include detailed failure reasons.

## Root Structure

- `simulation`: Global simulation run controls.
- `plugins`: Plugin loading and plugin-specific runtime parameters.
- `initial_state_file`: Input state file used to initialize simulation state.
- `output_directory`: Base directory for generated simulation outputs.
- `logging`: Logger output/retention behavior.
- `performance`: Performance instrumentation and execution tuning.
- `validation`: Runtime sanity and numerical safety checks.
- `vehicle_models`: Vehicle model data sources (aero, geometry, propulsion, mass, limits).
- `mission_environment`: Planet, mission sequence, atmosphere/wind, and integrator setup.

---

## `simulation`

- `simulation.duration` (`number`, seconds)  
  Maximum simulated time before normal stop condition.

- `simulation.max_iterations` (`integer`)  
  Maximum number of simulation ticks before forced stop condition.

- `simulation.enable_logging` (`boolean`)  
  Master on/off switch for simulation log emission.

- `simulation.log_file` (`string`, path)  
  File path for persisted logs.

- `simulation.log_level` (`string`)  
  Minimum severity level to emit (for example: `DEBUG`, `INFO`, `WARNING`, `ERROR`).

---

## `plugins[]`

Each entry in `plugins` configures one plugin.

- `plugins[].name` (`string`)  
  Logical plugin name used for identification in logs/UI.

- `plugins[].type` (`integer`)  
  Plugin execution role:
  - `0`: sequential state modifier (mutates state directly)
  - `1`: parallel physics calculator (outputs force/torque)

- `plugins[].library_path` (`string`)  
  Shared library path/name used to load the plugin binary.

- `plugins[].enabled` (`boolean`)  
  Enables/disables plugin loading and execution.

- `plugins[].parameters` (`object`)  
  Free-form plugin configuration passed into plugin `configure` API.

Example parameters in default config:
- `plugins[].parameters.step_size` (`number`)  
  Plugin-defined step factor (meaning owned by plugin implementation).
- `plugins[].parameters.debug_output` (`boolean`)  
  Plugin-defined verbose/debug behavior toggle.

---

## Global I/O

- `initial_state_file` (`string`, path)  
  Source file containing the initial simulation state.

- `output_directory` (`string`, path)  
  Destination root directory for simulation outputs/artifacts.

---

## `logging`

- `logging.console_output` (`boolean`)  
  If `true`, log messages are written to console/stdout.
  Currently integrated in core runtime through `Logger::setConsoleOutputEnabled(...)`.

- `logging.file_output` (`boolean`)  
  If `true`, log messages are written to configured log file.
  Currently integrated in core runtime through `Logger::setLogFile(...)` / `Logger::closeLogFile()`.

- `logging.log_rotation` (`boolean`)  
  Enables rolling/rotating log files when size limit is reached.

- `logging.max_file_size_mb` (`number`, MB)  
  Max size of one log file before rotation.

- `logging.max_files` (`integer`)  
  Number of rotated log files retained.

---

## `performance`

- `performance.enable_metrics` (`boolean`)  
  Enables collection of performance metrics (tick timing, plugin timing, etc.).

- `performance.metrics_output_interval` (`integer`)  
  Metric publication interval (typically every N ticks).

- `performance.enable_profiling` (`boolean`)  
  Enables deeper profiling/tracing hooks with higher overhead.

- `performance.thread_pool_size` (`integer`)  
  Worker thread count used for parallelizable runtime tasks.

---

## `validation`

- `validation.max_position_magnitude` (`number`)  
  Upper bound for position vector magnitude before warning/error policy is triggered.

- `validation.max_velocity_magnitude` (`number`)  
  Upper bound for velocity vector magnitude before warning/error policy is triggered.

- `validation.enable_nan_checks` (`boolean`)  
  Enables checks for NaN/Inf values in simulation state.

---

## `vehicle_models`

### `vehicle_models.aero_database`
- `type` (`string`)  
  Aerodynamic model type (for example `lookup_table`).
- `source.uri` (`string`, path/URI)  
  Location of aerodynamic data source.
- `source.format` (`string`)  
  Data format (for example `hdf5`).
- `axes` (`array<string>`)  
  Independent variables used by aero lookup/interpolation (for example `alpha_deg`, `beta_deg`, `mach`).

### `vehicle_models.reference_geometry`
- `source.uri` (`string`, path/URI`)  
  Geometry dataset location.
- `source.format` (`string`)  
  Geometry data format (for example `csv`).

### `vehicle_models.propulsion_model`
- `source.uri` (`string`, path/URI`)  
  Propulsion performance dataset location.
- `source.format` (`string`)  
  Propulsion data format (for example `csv`).

### `vehicle_models.mass_properties`
- `source.uri` (`string`, path/URI`)  
  Mass/inertia dataset location.
- `source.format` (`string`)  
  Mass properties format (for example `json`).

### `vehicle_models.physical_limits`
- `actuators[]` (`array<object>`)  
  Per-actuator command/rate constraints.
  - `actuators[].id` (`string`)  
    Actuator identifier.
  - `actuators[].max_rate_deg_s` (`number`, deg/s)  
    Maximum deflection rate.
  - `actuators[].range_deg` (`number`, deg)  
    Maximum absolute deflection range.
- `structural.source.uri` (`string`, path/URI`)  
  Structural constraints dataset location.
- `structural.source.format` (`string`)  
  Structural limits format (for example `csv`).

---

## `mission_environment`

### `mission_environment.planet_model`
- `planet_model` (`string`)  
  Planet/geodesy model selection (for example `WGS84`).

### `mission_environment.flight_sequence`
- `source.uri` (`string`, path/URI`)  
  Mission/phase sequence definition file.
- `source.format` (`string`)  
  Sequence format (for example `yaml`).

### `mission_environment.atmosphere_wind`
- `model` (`string`)  
  Atmosphere model selection (for example `US_Standard_1976`).
- `wind_profile_source.uri` (`string`, path/URI`)  
  Wind profile dataset location.
- `wind_profile_source.format` (`string`)  
  Wind profile format (for example `csv`).

### `mission_environment.integrator_config`
- `method` (`string`)  
  Numerical integration method (for example `runge_kutta_4`).
- `rtol` (`number`)  
  Relative tolerance for adaptive integration/error control.
- `atol` (`number`)  
  Absolute tolerance for adaptive integration/error control.

---

## Notes

- File paths may be relative to the process working directory or repository root, depending on launcher/runtime.
- Units should be treated as mandatory contract where specified (seconds, deg/s, MB, etc.).
- `plugins[].parameters` keys are plugin-owned and may vary by plugin implementation.
