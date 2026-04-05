# Contrato de configuración actual

Fuente: `data/defaults/default_config.json` + `ConfigManager`.

## Parseado y usado por el núcleo

- `simulation.duration`
- `simulation.max_iterations`
- `simulation.enable_logging` (se parsea, pero no se aplica como switch global completo)
- `simulation.log_file`
- `simulation.log_level`
- `plugins[]`
  - `name`
  - `type`
  - `library_path`
  - `enabled`
  - `parameters`
    - `enable_host_logger_integration` (opcional, `boolean`, default `false`)
- `initial_state_file`
- `output_directory` (se parsea, pero todavía no se aplica completamente en la salida del motor)

## Presente en JSON pero aún no consumido por el runtime del núcleo

- `logging.*`
- `performance.*`
- `validation.*`
- `vehicle_models.*`
- `mission_environment.*`

## Validaciones actualmente aplicadas

- `simulation.duration > 0`
- `simulation.max_iterations > 0`
- nombre/ruta de plugin no vacíos
- tipo de plugin en `{0,1}`

## Comportamiento actual de logging por plugin

 - El host solo inyecta servicios de logging al plugin cuando `plugins[].parameters.enable_host_logger_integration` es `true`.
- La inyección es opcional y depende de que el plugin exporte `plugin_set_host_services`.
- Compatibilidad: también se acepta la clave legacy `use_host_logger`.

---

Estado de documentación: **Current**

Volver a: [`docs/README.md`](../README.md)
