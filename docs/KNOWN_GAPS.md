# Known gaps (current)

## Configuration/runtime integration gaps

1. `output_directory` is parsed, but engine output still uses a hardcoded path.
2. `simulation.enable_logging` is parsed, but not used as a global on/off switch.
3. `logging.console_output` and `logging.file_output` are integrated; remaining logging keys (`logging.log_rotation`, `logging.max_file_size_mb`, `logging.max_files`) are not integrated with logging backend.
4. `performance.*` section is not integrated with scheduler/metrics controls.
5. `validation.*` thresholds are not integrated in runtime validation.
6. `vehicle_models.*` is unused.
7. `mission_environment.*` is unused.

## UI/backend contract gaps

- The web UI sends physics/output sections that current `ConfigManager` does not parse.
- Port mismatch:
  - launcher message: `8080`
  - Python server: `8082`

---

Documentation status: **Current**

Back to: [`docs/README.md`](../README.md)
