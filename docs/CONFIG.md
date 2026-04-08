# Current configuration contract

Source: `data/defaults/default_config.json` + `ConfigManager`.

## Load lifecycle

- Configuration is loaded once during application bootstrap in `main.cpp`.
- `SimulationEngine::initialize_with_config()` consumes the preloaded `ConfigManager` state and does not reload the file.
- `ConfigManager` emits no logs before a successful config load.
- If config loading fails, `ConfigManager` forces full logging (console enabled, logger verbosity raised) and emits detailed failure diagnostics.
- On successful load, `ConfigManager` applies `logging.console_output` to the logger before emitting its success message.
- On failure, `ConfigManager` emits the exact parse/validation failure detail (not a generic message).

## Parsed and used by the core

- `simulation.duration`
- `simulation.max_iterations`
- `simulation.enable_logging` (parsed, but not applied as a full global on/off switch)
- `simulation.log_file`
- `simulation.log_level`
- `plugins[]`
  - `name`
  - `type`
  - `library_path`
  - `enabled`
  - `parameters`
    - `enable_host_logger_integration` (optional, `boolean`, default `false`)
- `initial_state_file`
- `output_directory` (parsed, but still not fully applied in engine output)
- `logging.console_output` (applied to logger console/stdout emission)

## Present in JSON but not yet consumed by core runtime

- `logging.file_output`
- `logging.log_rotation`
- `logging.max_file_size_mb`
- `logging.max_files`
- `performance.*`
- `validation.*`
- `vehicle_models.*`
- `mission_environment.*`

## Currently applied validations

- `simulation.duration > 0`
- `simulation.max_iterations > 0`
- non-empty plugin name/path
- plugin type in `{0,1}`

## Current plugin logging behavior

 - Host injects logging services only when `plugins[].parameters.enable_host_logger_integration` is `true`.
- Injection is optional and depends on plugin exporting `plugin_set_host_services`.
- Compatibility: legacy key `use_host_logger` is also accepted.

## Logger extensibility

- `Logger` supports an optional structured sink callback through `setStructuredSink(...)`.
- The sink receives raw fields `(LogLevel, message, component)` for each accepted log event.

---

Documentation status: **Current**

Back to: [`docs/README.md`](../README.md)
