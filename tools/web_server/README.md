# MoLab Web Server

Backend/frontend module to run simulations and visualize results from a browser.

## Purpose

- Expose HTTP endpoints for simulation control.
- Serve static web interface assets.
- Query status, progress, and execution results.

## Structure

```text
tools/web_server/
├── config.py
├── logger.py
├── router.py
├── server.py
├── simulation_manager.py
├── utils.py
├── handlers/
│   ├── base_handler.py
│   ├── static_handler.py
│   └── api_handlers.py
└── static/
    ├── index.html
    ├── css/
    └── js/
```

## Execution

Desde la raiz del repositorio:

```bash
python tools/molab_web_gui_v2.py
```

Common options:

```bash
python tools/molab_web_gui_v2.py --port 8080
python tools/molab_web_gui_v2.py --no-browser
python tools/molab_web_gui_v2.py --verbose
```

## Main endpoints

- `GET /api/status`
- `GET /api/plugins`
- `POST /api/run`
- `GET /api/simulation/status/{job_id}`
- `POST /api/simulation/cancel/{job_id}`
- `GET /api/results`
- `GET /api/results/{filename}`

## Maintenance notes

- Keep clear separation between handlers, simulation logic, and static frontend.
- Avoid business logic in presentation layers.
- Prioritize explicit error handling and actionable log messages.
