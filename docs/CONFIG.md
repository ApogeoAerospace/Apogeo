# Current configuration contract

Source: `data/defaults/default_config.json` + `ConfigManager`.

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

## Present in JSON but not yet consumed by core runtime

- `logging.*`
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

---

Documentation status: **Current**

Back to: [`docs/README.md`](../README.md)
