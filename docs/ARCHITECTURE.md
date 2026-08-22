# Current architecture (implemented)

## Main runtime components

- `SimulationEngine`
- `ConfigManager`
- `PluginManager`
- `PhysicsIntegrator`
- `OutputManager`
- `TimeManager`
- `InitialStateLoader`
- `Logger`

## Main execution flow

1. `main.cpp` processes command-line options.
2. `main.cpp` loads configuration once through `ConfigManager::loadConfig()`.
3. Logger bootstrap is applied from loaded config (level, file, `logging.console_output`, `logging.file_output`).
   - Ownership is in `main.cpp` (single source of runtime side effects).
   - `ConfigManager` remains focused on config data loading/validation.
4. `SimulationEngine::initialize_with_config()` consumes preloaded config.
5. `ConfigManager` parses:
   - `simulation`
   - `plugins`
   - `initial_state_file`
   - `output_directory`
6. `PluginManager::load_plugins_from_config()` loads enabled plugins.
   - If plugin exports `plugin_set_host_services` and `use_host_logger=true`, the host injects logging services.
7. Engine initializes initial state and runs ticks.
8. Tick cycle:
   - sequential plugins (`type=0`)
   - parallel plugins (`type=1`)
   - physics integration
   - output record

Optional control path:

- `main.cpp` supports `--ipc stdio` mode for JSON-line command handling over stdin/stdout.
- IPC commands are focused on runtime control (`initialize`, `run_ticks`, `run_full`, `shutdown`, `get_status`).
- Configurable startup behavior remains file-based through `default_config.json` + CLI (`--config`).
- IPC mode streams structured events (`log`, `error`, `tick_completed`, `simulation_started`, `simulation_finished`, `state_sample`).

## Plugin roles

- `0` = `SEQUENTIAL_STATE_MODIFIER`
- `1` = `PARALLEL_PHYSICS_CALCULATOR`

## Notes

- Current plugin scheduler uses threads (`PluginTaskScheduler`).
- `SimulationEngine` still has partially hardcoded output behavior.
- `PluginManager` optionally resolves `plugin_set_host_services` during load (`load_plugin`) and applies config-based activation in `load_plugins_from_config`.
- Simulation-state environment data such as `gravity` remains part of `GeneralState` and is preserved when state buffers are rebuilt; it is not a `PluginManager` responsibility.
- `Logger` provides an optional structured sink callback via `setStructuredSink(...)`.
  - The callback receives `(LogLevel, message, component)` for each emitted log event.
  - If no sink is configured, console/file logging only is used.
- `ConfigManager::logLoadFailure(...)` is a deliberate failure-path diagnostic exception,
  used to surface config load errors when bootstrap cannot rely on loaded settings.
