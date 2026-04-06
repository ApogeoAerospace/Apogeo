# Lista de Tareas — CD-84: Actualización de Visualización de Datos

**Rama:** `feature/cd-84-actualizacion-de-visualizacion-de-datos`  
**Fecha:** 2026-02-12  
**Referencia:** [Spec Funcional](./functional_spec.md) | [Spec Técnica](./technical_spec.md)

---

## Tareas

### T-01: Implementar script base con CLI y carga de datos ✅

- **Prioridad:** Alta
- **Descripción:** Crear `tools/visualize_results.py` con:
  - `parse_arguments()` — argumentos `--file`, `--output-dir`, `--plots-dir`, `--no-show`.
  - `load_csv_data()` — carga CSV más reciente o archivo específico con pandas.
  - `load_metadata()` — carga JSON metadata opcional.
  - `compute_derived_columns()` — calcula `speed`, `downrange`, `altitude`.
  - `main()` — punto de entrada.
- **Criterio de completitud:** El script se ejecuta, carga un CSV correctamente e imprime un resumen básico.
- **Ref:** Spec Técnica §4.1

---

### T-02: Implementar los 6 gráficos ✅

- **Prioridad:** Alta
- **Dependencia:** T-01
- **Descripción:** Implementar las 6 funciones de gráficos:
  - `plot_trajectory_3d()` — Trayectoria 3D con colormap por tiempo.
  - `plot_altitude_vs_time()` — Altitud vs tiempo.
  - `plot_speed_vs_time()` — Magnitud de velocidad vs tiempo.
  - `plot_velocity_components()` — Vx, Vy, Vz superpuestas.
  - `plot_atmospheric()` — Panel 3x1: densidad, presión, temperatura.
  - `plot_trajectory_2d()` — Downrange vs altitud.
- **Criterio de completitud:** Se generan 6 archivos PNG en `output/plots/`.
- **Ref:** Spec Técnica §4.1 — Gráficos generados

---

### T-03: Implementar orquestador y modo `--no-show` ✅

- **Prioridad:** Alta
- **Dependencia:** T-02
- **Descripción:** Implementar `generate_all_plots()` que llama a todos los gráficos, gestiona el directorio de salida, guarda PNGs y controla `plt.show()`. Configurar backend `Agg` cuando corresponda.
- **Criterio de completitud:** `python tools/visualize_results.py --no-show` genera los 6 PNGs sin abrir ventanas.
- **Ref:** Spec Técnica §4.1

---

### T-04: Manejo de errores y casos borde ✅

- **Prioridad:** Alta
- **Dependencia:** T-03
- **Descripción:** Manejo robusto de errores:
  - CSV no encontrado → `sys.exit(1)` con mensaje.
  - CSV vacío → `sys.exit(1)` con mensaje.
  - Columnas faltantes → warning + omitir gráfico afectado.
  - Metadata ausente → continuar con defaults.
- **Criterio de completitud:** El script no lanza excepciones no controladas en ningún caso borde.
- **Ref:** Spec Técnica §4.1 — Manejo de errores

---

### T-05: Integrar generación automática de plots en `molab_web_gui.py` ✅

- **Prioridad:** Alta
- **Dependencia:** T-03
- **Descripción:** Modificar `run_sim()` en `molab_web_gui.py` para ejecutar `visualize_results.py --no-show` automáticamente después de que la simulación termina con éxito. Best-effort con timeout de 60s.
- **Criterio de completitud:** Al ejecutar una simulación desde la web, los PNGs se generan automáticamente en `output/plots/`.
- **Ref:** Spec Técnica §4.2 — Backend: `run_sim()`

---

### T-06: Agregar API REST para servir plots ✅

- **Prioridad:** Alta
- **Dependencia:** T-05
- **Descripción:** Agregar dos endpoints en `molab_web_gui.py`:
  - `GET /api/plots` — lista PNGs disponibles (JSON).
  - `GET /api/plots/<filename>` — sirve archivo PNG con `Content-type: image/png`.
- **Criterio de completitud:** Los endpoints responden correctamente y sirven los archivos PNG.
- **Ref:** Spec Técnica §4.2 — Backend: `serve_plots_list()`, `serve_plot_file()`

---

### T-07: Agregar galería de descarga en el frontend ✅

- **Prioridad:** Alta
- **Dependencia:** T-06
- **Descripción:** Modificar el HTML/JS embebido en `molab_web_gui.py`:
  - Agregar contenedor `#plots-gallery` con grid responsive.
  - Implementar función `loadPlots()` que carga la lista de plots y renderiza cards con thumbnail, etiqueta y botón de descarga.
  - Modificar `completeSimulation()` para llamar a `loadPlots()` al finalizar.
- **Criterio de completitud:** Al completar una simulación, la galería muestra los 6 gráficos con opción de descarga.
- **Ref:** Spec Técnica §4.2 — Frontend

---

### T-08: Verificación final ✅

- **Prioridad:** Alta
- **Dependencia:** T-07
- **Descripción:** Pruebas de integración:
  - [x] Script independiente: `python tools/visualize_results.py --no-show` genera 6 PNGs.
  - [x] Script con archivo específico: `--file` funciona correctamente.
  - [x] Integración web: simulación desde la web genera plots automáticamente.
  - [x] Galería web: muestra los 6 gráficos con descarga.
  - [x] API: `/api/plots` lista PNGs, `/api/plots/<filename>` sirve archivos.
- **Criterio de completitud:** Todos los checks pasan sin errores.

---

## Resumen

| Tarea | Descripción                                | Prioridad | Estado |
|-------|--------------------------------------------|-----------|--------|
| T-01  | Script base (CLI + carga de datos)         | Alta      | ✅     |
| T-02  | 6 gráficos (3D, altitud, velocidad, etc.)  | Alta      | ✅     |
| T-03  | Orquestador y modo --no-show               | Alta      | ✅     |
| T-04  | Manejo de errores                          | Alta      | ✅     |
| T-05  | Generación automática post-simulación      | Alta      | ✅     |
| T-06  | API REST para servir plots                 | Alta      | ✅     |
| T-07  | Galería de descarga en frontend            | Alta      | ✅     |
| T-08  | Verificación final                         | Alta      | ✅     |

**Total: 8 tareas — Todas completadas**
