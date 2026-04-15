# Current configuration contract

Source: `data/defaults/default_config.json` + `ConfigManager`.

## Load lifecycle

- Configuration is loaded once during application bootstrap in `main.cpp`.
- `SimulationEngine::initialize_with_config()` consumes the preloaded `ConfigManager` state and does not reload the file.
- `ConfigManager` is focused on loading/parsing/validation and exposing configuration data.
- Logger bootstrap side effects (`logging.console_output`, `logging.file_output`, `simulation.log_level`) are applied in `main.cpp`.
- If config loading fails, defaults are applied and `ConfigManager` emits detailed failure diagnostics through `logLoadFailure(...)`.
- On successful load, `main.cpp` emits the success status after applying logger bootstrap policy.

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
- `logging.file_output` (applied to logger file emission)
- `ipc.tick_event_interval` (applied in IPC mode as default `tick_completed` throttle)
- `ipc.telemetry_interval_ticks` (applied in IPC mode as default `state_sample` throttle)

## Present in JSON but not yet consumed by core runtime

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
- `Logger` keeps a bounded in-memory history of accepted log events for IPC startup replay.
- Current replay history cap: `256` entries.
- In `--ipc stdio` mode, console/stdout text logging is forcibly disabled to preserve JSON-line framing, regardless of `logging.console_output`.

## Architecture note

- Keep configuration ownership split intentionally:
  - `ConfigManager`: data contract (load/parse/validate/defaults).
  - `main.cpp`: runtime bootstrap side effects (logger policy, CLI overrides, IPC-specific logger behavior).

---

Documentation status: **Current**

Back to: [`docs/README.md`](../README.md)
