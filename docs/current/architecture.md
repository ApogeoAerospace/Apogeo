# Arquitectura actual (implementado)

## Componentes principales en runtime

- `SimulationEngine`
- `ConfigManager`
- `PluginManager`
- `PhysicsIntegrator`
- `OutputManager`
- `TimeManager`
- `InitialStateLoader`
- `Logger`

## Flujo principal de ejecución

1. `main.cpp` procesa opciones de línea de comandos.
2. `SimulationEngine::initialize_with_config()` carga la configuración.
3. `ConfigManager` parsea:
   - `simulation`
   - `plugins`
   - `initial_state_file`
   - `output_directory`
4. `PluginManager::load_plugins_from_config()` carga plugins habilitados.
5. El motor inicializa el estado inicial y ejecuta ticks.
6. Ciclo de tick:
   - sequential plugins (`type=0`)
   - parallel plugins (`type=1`)
   - physics integration
   - output record

## Roles de plugins

- `0` = `SEQUENTIAL_STATE_MODIFIER`
- `1` = `PARALLEL_PHYSICS_CALCULATOR`

## Notas

- El planificador actual de plugins usa hilos (`PluginTaskScheduler`).
- `SimulationEngine` todavía tiene parte del comportamiento de salida hardcodeado.
