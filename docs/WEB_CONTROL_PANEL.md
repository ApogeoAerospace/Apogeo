# Web Control Panel

> Status: **Current** (matches `develop` + the changes of this PR)

The web control panel is a local development tool to configure, run and inspect Apogeo
simulations from a browser. It is a thin layer over the simulator: it never computes physics
itself, it starts `simulator --ipc stdio` and talks to it through the
[IPC protocol](./IPC_PROTOCOL.md).

## Contents

1. [Quick start](#quick-start)
2. [Architecture](#architecture)
3. [What the panel can do](#what-the-panel-can-do)
4. [Plugin management](#plugin-management)
5. [Gravity](#gravity)
6. [HTTP API reference](#http-api-reference)
7. [Job lifecycle and IPC mapping](#job-lifecycle-and-ipc-mapping)
8. [Security notes](#security-notes)
9. [Testing](#testing)
10. [Troubleshooting](#troubleshooting)

## Quick start

Build the simulator first (see [BUILD_RUN.md](./BUILD_RUN.md)), then from the repository root:

```bash
python tools/web_gui.py                 # http://localhost:8082, opens the browser
python tools/web_gui.py --port 8090 --no-browser --verbose
```

`scripts/launch_web.sh` and `scripts/launch_web.ps1` do the same after checking Python.

The server looks for the simulator in this order (first directory that contains it wins):

1. `Apogeo_BUILD_DIR` (environment variable)
2. `<repo>/build`
3. `<repo>/out/build` (Visual Studio / CMake presets)

Plugin libraries are searched next to the simulator and in the usual `plugins/`, `bin/`, `lib/`
sub-directories of the build directory, so both single-config and multi-config layouts work.

## Architecture

```text
 Browser (static/)                Python server (tools/web_server/)               Simulator (C++)
 ┌───────────────────┐  HTTP    ┌───────────────────────────────────┐  stdio    ┌──────────────────┐
 │ app.js  ui.js     │ ───────► │ router.py ─► handlers/api_handlers │  JSON     │ simulator        │
 │ simulation.js     │  JSON    │   PluginRegistry (plugin state)    │  lines    │   --ipc stdio    │
 │ results.js charts │ ◄─────── │   SimulationManager (jobs, IPC)    │ ◄───────► │   plugins (.so)  │
 └───────────────────┘ polling  └───────────────────────────────────┘           └──────────────────┘
```

- **There are no WebSockets.** The browser polls `GET /api/simulation/status/{job_id}` every
  250 ms while a job runs. Everything that looks "live" (progress, simulator log) comes from
  that polling.
- **The backend is the single source of truth** for plugins. The browser never keeps its own
  copy of "what is enabled": every switch change is a request, and the UI redraws from the
  response (or re-reads the list if the request failed).
- Each run starts a fresh simulator process with the project root as working directory, so
  relative paths in configs and plugins resolve the same way no matter where the server was
  launched from.

### Source layout

| Path | Responsibility |
|---|---|
| `tools/web_gui.py`, `tools/web_server/__main__.py` | Entry points |
| `tools/web_server/server.py`, `router.py` | Threaded HTTP server, routes, CORS preflight, body limit |
| `tools/web_server/plugin_registry.py` | Plugin list, toggle, parameters, presets, gravity providers |
| `tools/web_server/simulation_manager.py` | Jobs, simulator process, IPC conversation, cancellation |
| `tools/web_server/handlers/` | HTTP handlers (`api_handlers.py`, `static_handler.py`) |
| `tools/web_server/utils.py` | Config validation/normalization, gravity resolution |
| `tools/web_server/static/` | Front-end (`app.html`, `js/`, `css/`, `fragments/`) |
| `tools/web_server/tests/` | API, browser and unit tests (see [Testing](#testing)) |

## What the panel can do

| Area | Capability |
|---|---|
| Simulation | Time step, duration (iterations derived), logging, integrator, atmospheric drag flag |
| Gravity | Seed `GeneralState.gravity` (enable, magnitude, direction) and see which plugin applies it |
| Plugins | Enable/disable, edit parameters, apply a template's plugin set, reset to defaults |
| Missions | Templates (initial state, physics, plugin preset) |
| Execution | Start, follow progress, cancel; warnings before the run; simulator log (`log` IPC events) |
| Results | List CSV outputs, plot trajectory/velocity/altitude/speed/atmosphere/gravity |
| Configs | Save and load configurations as JSON |

What it deliberately does **not** do (the protocol does not allow it today, see
[IPC_PROTOCOL_PROPOSALS.md](./IPC_PROTOCOL_PROPOSALS.md)): pause/resume, change anything
while a run is in progress, read the live state, or discover plugin parameters from the
plugins themselves.

## Plugin management

### Where the plugin list comes from

`PluginRegistry` merges three layers (lowest priority first):

1. **Base configuration**: `data/config/web_last_config.json` if present, otherwise
   `data/defaults/default_config.json`.
2. **Overlay** `data/config/web_plugin_state.json`: the `enabled` flag and parameter overrides
   chosen in the panel. It is git-ignored and written atomically. Resetting deletes it.
3. **Build information**: whether the plugin library exists (`library_found`) and where.

The versioned defaults are **never rewritten** by toggling a plugin.

```json
{ "version": 1, "plugins": { "example_plugin": { "enabled": true, "parameters": { "apply_gravity": true } } } }
```

### Toggle semantics

- `POST /api/plugins/{id}/toggle` with `{"enabled": true|false}` is idempotent. Without a
  body it flips the current state (kept for scripts; the UI always sends an explicit value).
- Enabling a plugin whose library is not built returns **409 `library_not_found`** and changes
  nothing. Disabling it is always allowed. The UI shows such plugins with a "Not built" badge
  and a disabled switch.
- Parameters are validated against the defaults: unknown keys are rejected, and the JSON type
  must match (`true` is not a number; `NaN`/`Infinity` are rejected).
- If any request fails, the UI shows the backend's message in the status bar and re-reads the
  list, so the switch can never show a state the backend does not have.

### How a run picks its plugins

`POST /api/run` does **not** need a `plugins` field: the plugin set is taken from the registry.
A legacy client may still send `plugins`; it overrides the registry for that run only and is
validated first (unknown plugin → 400, unbuilt plugin → 409). The paths sent to the simulator are
resolved library files, never source directories.

### Templates

Applying a template calls `POST /api/plugins/preset` once. The enabled set is **replaced**
(plugins not listed are disabled). Listed plugins that are not built are skipped and reported
(`skipped: true`) instead of failing the template.

## Gravity

### What the core does and does not do

The core **carries** gravity: `GeneralState.gravity` is loaded from the initial state, preserved
when the state buffer is rebuilt and written to the output CSV (`gravity_x/y/z`). **Nothing in
the core applies it to the dynamics**: the integrator only uses the forces plugins return. A
trajectory is affected by gravity only if an enabled plugin turns it into a force.

### Who applies it

| Plugin | Mode | Behaviour |
|---|---|---|
| `example_plugin` with `apply_gravity: true` | `state_vector` | Force = `mass × gravity × gravity_scale`, reading `GeneralState.gravity` (what the panel seeds). Needs `"type": 1`. |
| `environment` (legacy module) | `own_model` | Computes gravity itself from the position (inverse-square, towards the Earth's centre) and **ignores** `GeneralState.gravity`. It has no switch to turn gravity off. |

The panel tells you which case applies and warns about the three situations that usually go
wrong:

- gravity enabled but **no plugin applies it** (nothing will fall);
- **two plugins apply it** (forces add up, the vehicle falls twice as fast);
- gravity "off" in the panel while an `own_model` plugin is enabled (it keeps applying it), or a
  custom magnitude/direction that an `own_model` plugin ignores.

The same checks run on the server when a run starts and are returned as `warnings` in the
`POST /api/run` response and in the job status.

### Request payload

The current contract is the `environment.gravity` block. The legacy `physics.enable_gravity` and
`physics.gravity_magnitude` fields are still accepted (older saved configs); when both exist,
`environment.gravity` wins.

```json
{
  "environment": {
    "gravity": { "enabled": true, "magnitude": 9.81, "direction": [0, 0, -1] }
  }
}
```

| Field | Rule |
|---|---|
| `enabled` | boolean |
| `magnitude` | finite number in `[0, 50]` m/s² |
| `direction` | 3 finite numbers, not all zero; normalized by the server |

The server resolves the effective vector (`direction × magnitude`, or `[0,0,0]` when disabled)
and writes it into the initial state file (`gravity: {x, y, z}`) before starting the simulator.
`GET /api/simulation/status/{job_id}` reports it under `gravity` together with its `source`
(`environment`, `legacy_physics` or `default`).

Plugin parameters are plugin-owned: the server does not add gravity (or any other) keys to them.

## HTTP API reference

All bodies are JSON. Errors from the plugin and run endpoints use
`{"success": false, "error": "...", "code": "..."}`.

### System

| Method | Path | Description |
|---|---|---|
| GET | `/api/status` | Simulator location and state: `simulator_built`, `simulator_path`, `simulator_exists`, `simulator_executable`, `project_root`, `build_dir`, `platform`, `timestamp` |
| GET | `/api/environment` | Default gravity, `gravity_providers` (`[{id, mode}]`), `gravity_consumers` (ids), `applies_gravity`, `gravity_conflict`, `environment_plugin` (`available`, `enabled`) |

### Plugins

| Method | Path | Body | Result |
|---|---|---|---|
| GET | `/api/plugins` | | List of `{id, name, description, notes, type, enabled, parameters, default_enabled, default_parameters, library_path, library_found}` |
| POST | `/api/plugins/{id}/toggle` | `{"enabled": bool}` (optional) | `{success, plugin}`. 404 `unknown_plugin`, 409 `library_not_found`, 400 |
| POST | `/api/plugins/{id}/parameters` | `{"parameters": {...}}` | `{success, plugin}`. 400 `unknown_parameter` / `invalid_parameters` |
| POST | `/api/plugins/preset` | `{"enabled": [ids], "overrides": {id: {...}}}` | `{success, plugins}` with `skipped` flags. 404 for unknown ids (nothing changes) |
| POST | `/api/plugins/reset` | | `{success, plugins}` (defaults restored) |

### Simulation and results

| Method | Path | Description |
|---|---|---|
| POST | `/api/run` | Validate, start a job. Returns `{success, job_id, warnings}` |
| GET | `/api/simulation/status/{job_id}` | `{id, status, progress, message, start_time, end_time, error, output_files, warnings, plugins, gravity, log_count, logs}` (`logs` = last 50 IPC `log` events) |
| POST | `/api/simulation/cancel/{job_id}` | Stops a pending/initializing/running job; 400 if it already finished |
| GET | `/api/results` | CSV result files found |
| GET | `/api/results/{filename}` | Parsed CSV (`headers`, `rows`) |

### Configuration

| Method | Path | Description |
|---|---|---|
| POST | `/api/config/save` | Save a config. If it has no `plugins`, the effective registry state is stored |
| GET | `/api/config/load` | List saved configs |
| GET | `/api/config/{filename}` | Read one saved config |

## Job lifecycle and IPC mapping

Job statuses: `pending → initializing → running → completed | failed | cancelled`.

For each job the manager writes a temporary config and initial state (unique names, removed
afterwards) and runs:

| Step | Command | Wait limit | Events consumed |
|---|---|---|---|
| 1 | `initialize` | 60 s | `log` |
| 2 | `run_full` (`tick_event_interval`, `telemetry_interval_ticks`) | `max_simulation_timeout` (600 s) | `simulation_started`, `tick_completed` (progress), `log`, `error`, `simulation_finished` |
| 3 | `shutdown` | 15 s, best effort | |

Details that matter when debugging:

- Waiting on the simulator is **bounded and cancellable**. A simulator that stops answering
  fails the job with "did not answer ... within Ns" and the process is terminated.
- If `run_full` fails while running, the protocol sends an `error` event and
  `simulation_finished` with `success: false` but **no ack**. The manager treats that event as
  the failure of the command, so the job ends as `failed` with the simulator's message.
- Cancelling terminates the child process and the job ends as `cancelled` (not `failed`).
- `stderr` is drained continuously and its tail is used as the error text when the process
  dies without a message.
- `state_sample` events exist in the protocol but are not consumed yet (results are read from
  the CSV after the run).

## Security notes

- The API can start processes and write files, so it is meant for **local development**.
  CORS answers only for an allow-list (`http://localhost:<port>`, `http://127.0.0.1:<port>`);
  there is no `Access-Control-Allow-Origin: *`. The bundled UI is same-origin and needs no CORS.
  Pass `allowed_origins` to `ServerConfig` when serving the UI from another dev server.
- Request bodies above 1 MB are rejected with 413.
- Static files are served only from `static/` (path traversal is blocked) and saved file names
  are sanitized.
- The server listens on **all interfaces** (as before). Do not expose it on an untrusted network.

## Testing

```bash
# Everything that does not need a C++ build
python -m unittest discover -s tools/web_server/tests -v      # or: pytest tools/web_server/tests
```

| File | What it covers |
|---|---|
| `tests/test_web_api.py` | Real server on a free port against a scratch project tree: plugin toggle/parameters/preset/reset, persistence, 409/404/400 cases, gravity resolution and providers, warnings, runs, cancellation, failing/hanging simulator, CORS, body limit, static assets |
| `tests/test_browser_smoke.py` | The real front-end in Chromium (Playwright): clean console on load, switch changes the backend and survives a reload, gravity notices, a run started from the UI. Skipped when Playwright/Chromium are missing (`pip install playwright && playwright install chromium`) |
| `tests/fake_simulator.py` | Stand-in simulator that speaks the real `--ipc stdio` protocol and records what it received. Tests run it with `ServerConfig(simulator_command=[sys.executable, "fake_simulator.py"])`, so no `.exe`/shebang is needed on any OS |
| `tests/test_example_plugin_physics.cpp` | (C++, in `core_tests`) force model of the Example Plugin |

`PlatformLayoutTests` builds the folder layouts CMake produces on Linux, macOS and Windows (Ninja, MinGW,
MSVC multi-config and the Visual Studio preset layout `out/build/x64-Debug`) and checks that the simulator
(`simulator` / `simulator.exe`) and the plugin libraries (`libX.so` / `libX.dylib` / `X.dll`) are found.
The test fixtures also name files per platform, so the whole suite is meant to run on any OS.

What these tests **do not** cover: the real C++ simulator end to end, drawing of the charts
(Chart.js is stubbed because it comes from a CDN), and process handling on Windows (they were developed on Linux;
the Windows naming is exercised by simulating the platform).

`ServerConfig(simulator_command=[...])` replaces the discovered simulator binary with any command (the server appends
`--config <file> --ipc stdio`). It exists for tests and for running the simulator under a debugger or wrapper.

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Switch is disabled with "Not built" | The plugin library is not in the build directory. Build the project, or set `Apogeo_BUILD_DIR`. The legacy Environment module is only built if it is listed in `DEPRECATED_PLUGIN_NAMES` in `CMakeLists.txt` |
| Toggle shows an error with "library was not found" | The library disappeared after the page loaded; the UI re-synced to the real (disabled) state |
| "Simulator not found" | Build first; check `GET /api/status` for the path searched |
| Gravity notice says nothing applies it | Enable the Example Plugin and turn on **Apply Gravity** (or build/enable the Environment module) |
| Gravity looks doubled | Two plugins apply it; keep only one (the panel warns about it) |
| Charts do not render, console shows a network error | Chart.js is loaded from `cdn.jsdelivr.net`; offline machines need the file served locally |
| "Port N is already in use" | Another instance is running; stop it or pass `--port` |

See also: [KNOWN_GAPS.md](./KNOWN_GAPS.md), [PLUGIN_API.md](./PLUGIN_API.md),
[CONFIG_REFERENCE.md](./CONFIG_REFERENCE.md).

---

Back to: [`docs/README.md`](./README.md)
