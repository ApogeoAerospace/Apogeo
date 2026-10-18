# MoLab Web Server

Local web control panel for MoLab. It starts the simulator in `--ipc stdio` mode, manages the
plugin set and shows progress and results. Full documentation:
[`docs/WEB_CONTROL_PANEL.md`](../../docs/WEB_CONTROL_PANEL.md).

## Structure

```text
tools/web_server/
├── config.py              # Server configuration, build directory discovery
├── logger.py
├── plugin_registry.py     # Plugin list, toggle, parameters, presets, gravity providers
├── router.py
├── server.py              # Threaded HTTP server, CORS preflight, body limit
├── simulation_manager.py  # Jobs, simulator process, IPC conversation
├── utils.py               # Config validation/normalization, gravity resolution
├── handlers/
│   ├── base_handler.py
│   ├── static_handler.py
│   └── api_handlers.py
├── static/                # Front-end (app.html, js/, css/, fragments/)
└── tests/                 # API, browser and unit tests, fake simulator
```

## Execution

From the repository root:

```bash
python tools/web_gui.py
```

Common options:

```bash
python tools/web_gui.py --port 8080
python tools/web_gui.py --no-browser
python tools/web_gui.py --verbose
```

The simulator is looked up in `MOLAB_BUILD_DIR`, then `build/`, then `out/build/`.

## Main endpoints

- `GET /api/status`
- `GET /api/environment`
- `GET /api/plugins`
- `POST /api/plugins/{id}/toggle`
- `POST /api/plugins/{id}/parameters`
- `POST /api/plugins/preset`
- `POST /api/plugins/reset`
- `POST /api/run`
- `GET /api/simulation/status/{job_id}`
- `POST /api/simulation/cancel/{job_id}`
- `GET /api/results`
- `GET /api/results/{filename}`
- `POST /api/config/save`, `GET /api/config/load`, `GET /api/config/{filename}`

Request/response details are in [`docs/WEB_CONTROL_PANEL.md`](../../docs/WEB_CONTROL_PANEL.md#http-api-reference).

## Tests

```bash
python -m unittest discover -s tools/web_server/tests -v
```

`test_browser_smoke.py` needs Playwright and Chromium and is skipped otherwise.

## Maintenance notes

- Keep clear separation between handlers, simulation logic, and static frontend.
- Avoid business logic in presentation layers.
- Prioritize explicit error handling and actionable log messages.
- The backend is the source of truth for plugin state; the front-end only renders it.
