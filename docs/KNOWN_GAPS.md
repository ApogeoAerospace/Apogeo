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

- The Environment module lives under `plugins/deprecated/environment/` and is not built: `DEPRECATED_PLUGIN_NAMES`
  in the root `CMakeLists.txt` is empty (listing `environment` there builds `environment.cpp` into `bin/plugins`).
  The `CMakeLists.txt` inside that directory is not used by the root build and is stale (it references
  `environment_plugin.cpp`, but the file is `environment.cpp`).
- Its `plugin_configure` ignores the JSON parameters, so `enable_atmospheric_model`, `enable_wind_effects`
  and `enable_gravity_variation` (listed in `data/defaults/web_config.json`) have no effect.
- It computes gravity with its own inverse-square model and ignores `GeneralState.gravity`;
  gravity cannot be switched off while the module is enabled.
- It uses a fixed `mass = 1000.0` kg for the gravity and Coriolis forces instead of `total_mass`
  (marked `TODO` in the source), so the resulting acceleration is wrong for any other vehicle mass.
- Enabling both the Environment module and `example_plugin` with `apply_gravity` applies gravity twice
  (the panel warns about it).
- `GET /api/plugins` is built from `default_config.json`; the plugin entries in `web_config.json` are not used for it.
- `data/config/web_last_config.json` is read as the preferred base configuration but nothing in the repository writes it.
- `POST /api/config/save` writes into `data/defaults/`, which is versioned: saving with the default name overwrites `web_config.json`.
- The panel loads Chart.js and the Inter font from CDNs; without internet the charts do not render.
- The web server listens on all network interfaces and has no authentication.
- The IPC command loop is sequential: a run cannot be paused, cancelled or queried while it executes
  (see [IPC_PROTOCOL_PROPOSALS.md](./IPC_PROTOCOL_PROPOSALS.md)).
- `tools/web_server/tests/test_example.py::TestUtilities::test_sanitize_filename` fails on Linux, also on
  unmodified `develop` (the backslash path case); it is expected to behave differently on Windows.

---

Documentation status: **Current**

Back to: [`docs/README.md`](./README.md)
