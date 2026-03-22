# Servidor web de MoLab

Módulo backend/frontend para ejecutar simulaciones y visualizar resultados desde navegador.

## Propósito

- Exponer endpoints HTTP para control de simulación.
- Servir interfaz web estática.
- Consultar estado, progreso y resultados de ejecuciones.

## Estructura

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

## Ejecución

Desde la raíz del repositorio:

```bash
python tools/molab_web_gui_v2.py
```

Opciones comunes:

```bash
python tools/molab_web_gui_v2.py --port 8080
python tools/molab_web_gui_v2.py --no-browser
python tools/molab_web_gui_v2.py --verbose
```

## Endpoints principales

- `GET /api/status`
- `GET /api/plugins`
- `POST /api/run`
- `GET /api/simulation/status/{job_id}`
- `POST /api/simulation/cancel/{job_id}`
- `GET /api/results`
- `GET /api/results/{filename}`

## Notas de mantenimiento

- Mantener separación clara entre handlers, lógica de simulación y frontend estático.
- Evitar lógica de negocio en capas de presentación.
- Priorizar manejo explícito de errores y mensajes de log útiles.
