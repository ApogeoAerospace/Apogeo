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

- Legacy web parsing path is deprecated in favor of `--ipc stdio` command/event protocol.
- The web UI sends physics/output sections that current `ConfigManager` does not parse.

## Web control panel and Environment module gaps
- **Deprecated Plugins:** The Environment module (`plugins/deprecated/environment/`) and other plugins in `plugins/deprecated/` are unvalidated prototype versions and are intentionally ignored for the web control panel and current simulation runs.
- `GET /api/plugins` is built from `default_config.json`; the plugin entries in `web_config.json` are not used for it.
- The panel loads Chart.js and the Inter font from CDNs; without internet the charts do not render.
- The web server listens on all network interfaces and has no authentication.
- The IPC command loop is sequential: a run cannot be paused, cancelled or queried while it executes (see [IPC_PROTOCOL_PROPOSALS.md](./IPC_PROTOCOL_PROPOSALS.md)).
- **Test Suite Execution:** On Linux environments, the test `tools/web_server/tests/test_example.py::TestServerConfig::test_port_validation` is known to fail due to local socket binding behavior. Test outputs have been updated to print clear summaries and explicit skip reasons for headless browser tests.
