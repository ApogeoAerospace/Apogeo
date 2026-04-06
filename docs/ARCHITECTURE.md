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
2. `SimulationEngine::initialize_with_config()` loads configuration.
3. `ConfigManager` parses:
   - `simulation`
   - `plugins`
   - `initial_state_file`
   - `output_directory`
4. `PluginManager::load_plugins_from_config()` loads enabled plugins.
   - If plugin exports `plugin_set_host_services` and `use_host_logger=true`, the host injects logging services.
5. Engine initializes initial state and runs ticks.
6. Tick cycle:
   - sequential plugins (`type=0`)
   - parallel plugins (`type=1`)
   - physics integration
   - output record

## Plugin roles

- `0` = `SEQUENTIAL_STATE_MODIFIER`
- `1` = `PARALLEL_PHYSICS_CALCULATOR`

## Notes

- Current plugin scheduler uses threads (`PluginTaskScheduler`).
- `SimulationEngine` still has partially hardcoded output behavior.
- `PluginManager` optionally resolves `plugin_set_host_services` during load (`load_plugin`) and applies config-based activation in `load_plugins_from_config`.
