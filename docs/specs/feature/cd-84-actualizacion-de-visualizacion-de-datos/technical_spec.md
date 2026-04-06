# Especificación Técnica — CD-84: Actualización de Visualización de Datos

**Rama:** `feature/cd-84-actualizacion-de-visualizacion-de-datos`  
**Fecha:** 2026-02-12  
**Estado:** Aprobada  
**Referencia:** [Especificación Funcional](./functional_spec.md)

---

## 1. Entorno Técnico

| Aspecto                              | Detalle                                       |
|--------------------------------------|-----------------------------------------------|
| **Lenguaje**                         | Python 3.8+ (entorno actual: 3.14.0)          |
| **Dependencias nuevas**              | `matplotlib`, `pandas`                        |
| **SO soportados**                    | Windows, Linux, macOS                         |
| **Directorio de entrada**            | `output/` (archivos `molab_simulation_*.csv`) |
| **Directorio de salida de gráficos** | `output/plots/`                               |

## 2. Estructura de Datos de Entrada

### 2.1 Formato CSV

Archivos con nombre `molab_simulation_YYYYMMDD_HHMMSS.csv`:

```
tick                 int      — Número de tick de simulación
simulation_time      float    — Tiempo de simulación en segundos
utc_time             float    — Timestamp UTC (epoch)
position_x           float    — Posición X (m)
position_y           float    — Posición Y (m)
position_z           float    — Posición Z (m)
velocity_x           float    — Velocidad X (m/s)
velocity_y           float    — Velocidad Y (m/s)
velocity_z           float    — Velocidad Z (m/s)
orientation_x        float    — Quaternion X
orientation_y        float    — Quaternion Y
orientation_z        float    — Quaternion Z
orientation_w        float    — Quaternion W
atm_density          float    — Densidad atmosférica (kg/m³)
atm_pressure         float    — Presión atmosférica (Pa)
atm_temperature      float    — Temperatura atmosférica (K)
gravity_x            float    — Gravedad X (m/s²)
gravity_y            float    — Gravedad Y (m/s²)
gravity_z            float    — Gravedad Z (m/s²)
wind_speed_x         float    — Viento X (m/s)
wind_speed_y         float    — Viento Y (m/s)
wind_speed_z         float    — Viento Z (m/s)
```

### 2.2 Metadata JSON (opcional)

Archivo `.json` con mismo nombre base. Contiene campo `metadata` con `run_name`, `timestamp`, `output_interval`. Se usa para títulos de gráficos si está disponible.

## 3. Componentes

La solución tiene dos componentes:

### 3.1 Script de visualización: `tools/visualize_results.py`

Script independiente que genera 6 gráficos PNG a partir de un CSV de simulación.

```
visualize_results.py
│
├── parse_arguments()          — CLI con argparse
├── load_csv_data()            — Carga CSV con pandas
├── load_metadata()            — Carga JSON metadata (opcional)
├── compute_derived_columns()  — Columnas calculadas (speed, downrange, altitude)
├── plot_trajectory_3d()       — Gráfico 1: Trayectoria 3D
├── plot_altitude_vs_time()    — Gráfico 2: Altitud vs Tiempo
├── plot_speed_vs_time()       — Gráfico 3: Velocidad vs Tiempo
├── plot_velocity_components() — Gráfico 4: Componentes de Velocidad
├── plot_atmospheric()         — Gráfico 5: Condiciones Atmosféricas
├── plot_trajectory_2d()       — Gráfico 6: Downrange vs Altitud
├── generate_all_plots()       — Orquestador de gráficos
└── main()                     — Punto de entrada
```

### 3.2 Integración web: `tools/molab_web_gui.py`

Modificaciones al servidor web monolítico para:
- Ejecutar `visualize_results.py` automáticamente al finalizar cada simulación.
- Servir los PNGs generados vía API REST.
- Mostrar una galería de gráficos con descarga en el frontend.

```
molab_web_gui.py (cambios)
│
├── run_sim()                  — Modificada: ejecuta visualize_results.py post-simulación
├── serve_plots_list()         — Nueva: GET /api/plots → lista de PNGs disponibles
├── serve_plot_file()          — Nueva: GET /api/plots/<filename> → sirve PNG
├── completeSimulation() [JS]  — Modificada: llama a loadPlots() al finalizar
└── loadPlots() [JS]           — Nueva: carga galería de plots con descarga
```

## 4. Detalle de Implementación

### 4.1 Script `visualize_results.py`

#### CLI

| Argumento      | Tipo   | Default          | Descripción                     |
|----------------|--------|------------------|---------------------------------|
| `--file`       | `str`  | `None`           | Ruta a un CSV específico        |
| `--output-dir` | `str`  | `"output"`       | Directorio donde buscar CSVs    |
| `--plots-dir`  | `str`  | `"output/plots"` | Directorio donde guardar PNGs   |
| `--no-show`    | `flag` | `False`          | No mostrar gráficos en pantalla |

#### Columnas derivadas

- `speed`: `sqrt(vx² + vy² + vz²)`
- `downrange`: `sqrt(px² + py²)`
- `altitude`: alias de `position_z` (Z es vertical, gravedad apunta en -Z)

#### Gráficos generados

| # | Función                    | Archivo                      | Descripción                                          |
|---|----------------------------|------------------------------|------------------------------------------------------|
| 1 | `plot_trajectory_3d()`     | `trajectory_3d.png`          | Trayectoria 3D con colormap por tiempo, marcadores   |
| 2 | `plot_altitude_vs_time()`  | `altitude_vs_time.png`       | Altitud vs tiempo                                    |
| 3 | `plot_speed_vs_time()`     | `speed_vs_time.png`          | Magnitud de velocidad vs tiempo                      |
| 4 | `plot_velocity_components()` | `velocity_components.png`  | Vx (rojo), Vy (verde), Vz (azul) vs tiempo          |
| 5 | `plot_atmospheric()`       | `atmospheric_conditions.png` | Panel 3x1: densidad, presión, temperatura vs tiempo  |
| 6 | `plot_trajectory_2d()`     | `trajectory_2d_profile.png`  | Perfil de vuelo: downrange vs altitud                |

#### Estilo visual

| Propiedad              | Valor                                                           |
|------------------------|-----------------------------------------------------------------|
| **Estilo matplotlib**  | `seaborn-v0_8-whitegrid` (con fallback a `ggplot`)              |
| **Tamaño de figura**   | `(12, 7)` individuales, `(12, 10)` subplots                    |
| **DPI de exportación** | 150                                                             |
| **Formato**            | PNG                                                             |

#### Manejo de errores

| Escenario                          | Comportamiento                                                       |
|------------------------------------|----------------------------------------------------------------------|
| No hay CSVs en el directorio       | `sys.exit(1)` con mensaje descriptivo                                |
| CSV vacío o con solo headers       | `sys.exit(1)` con mensaje descriptivo                                |
| Columnas faltantes en CSV          | Warning en consola, omitir gráfico afectado, continuar con los demás |
| JSON metadata no encontrado        | Continuar sin metadata, usar valores por defecto en títulos          |
| `--no-show` en entorno sin display | Funciona correctamente (usa backend `Agg`)                           |
| Directorio de plots no existe      | Crearlo automáticamente con `os.makedirs(..., exist_ok=True)`        |

### 4.2 Integración en `molab_web_gui.py`

#### Backend: `run_sim()` (modificada)

Después de que `subprocess.run()` del simulador termina con `returncode == 0`:

```python
subprocess.run([
    sys.executable, str(visualizer),
    "--output-dir", str(out_dir),
    "--plots-dir", str(plots_dir),
    "--no-show"
], capture_output=True, text=True, timeout=60)
```

- Best-effort: si falla, se imprime warning en consola pero la simulación sigue como exitosa.
- Timeout de 60 segundos.

#### Backend: `serve_plots_list()` (nueva)

- **Ruta**: `GET /api/plots`
- Busca archivos `*.png` en `output/plots/` dentro de los directorios de salida configurados.
- Retorna JSON array con `filename`, `size`, `modified`, `url`.

#### Backend: `serve_plot_file()` (nueva)

- **Ruta**: `GET /api/plots/<filename>`
- Sanitiza el filename con `os.path.basename()`.
- Solo acepta archivos `.png`.
- Sirve el archivo con `Content-type: image/png`.

#### Frontend: `completeSimulation()` (modificada)

- Muestra mensaje "Generating plots..." al completar.
- Espera 5 segundos para que los plots se generen.
- Llama a `loadResults()` y `loadPlots()`.
- Cambia a la pestaña Results.

#### Frontend: `loadPlots()` (nueva)

- Hace `fetch('/api/plots')`.
- Renderiza una galería en grid responsive con:
  - Thumbnail de cada gráfico (clickeable para ver tamaño completo).
  - Etiqueta descriptiva del gráfico.
  - Botón "⬇️ Download" con atributo `download`.

#### Frontend: HTML (nuevo contenedor)

- `#plots-gallery`: contenedor oculto por defecto, se muestra cuando hay plots.
- `#plots-grid`: grid CSS `repeat(auto-fill, minmax(300px, 1fr))`.
- Ubicado entre `#results-list` y `#chart-container`.

## 5. Dependencias

Instalación manual:

```bash
pip install matplotlib pandas
```

## 6. Archivos Creados/Modificados

| Acción        | Archivo                      | Descripción                                                  |
|---------------|------------------------------|--------------------------------------------------------------|
| **Crear**     | `tools/visualize_results.py` | Script principal de visualización (6 gráficos)               |
| **Modificar** | `tools/molab_web_gui.py`     | Generación automática de plots + API + galería de descarga   |

## 7. Testing Manual

```bash
# Script independiente — modo automático
python tools/visualize_results.py --no-show

# Script independiente — archivo específico
python tools/visualize_results.py --file output/molab_simulation_20251126_222234.csv --no-show

# Integración web — ejecutar servidor y correr simulación
python tools/molab_web_gui.py
# → Abrir http://localhost:8082, ejecutar simulación
# → Al completar: galería de plots visible con botones de descarga
```

Verificar que:
1. Se generan 6 archivos PNG en `output/plots/`.
2. Los gráficos tienen títulos, etiquetas y unidades correctas.
3. El modo `--no-show` no abre ventanas.
4. La galería web muestra los 6 gráficos con opción de descarga.
5. Funciona con CSVs de distintos tamaños (desde 263 bytes hasta ~1.3 MB).
