# Especificación Funcional — CD-84: Actualización de Visualización de Datos

**Rama:** `feature/cd-84-actualizacion-de-visualizacion-de-datos`  
**Fecha:** 2026-02-12  
**Estado:** Aprobada

---

## 1. Objetivo

Mejorar la forma en que el equipo de desarrollo visualiza los resultados de las simulaciones de prueba. En lugar de solo generar archivos CSV, se crea un script estandarizado que carga automáticamente el archivo de salida y genera un conjunto de gráficos clave, integrado directamente en la interfaz web para que los gráficos se generen y estén disponibles para descarga al finalizar cada simulación.

## 2. Contexto y Problema Actual

El simulador MoLab genera archivos CSV y JSON en el directorio `output/`. El script existente `tools/analyze_results.py` solo produce un resumen textual en consola. Para visualizar gráficamente los datos, el desarrollador debe abrir manualmente el CSV en herramientas externas o escribir scripts ad-hoc. Esto es lento, no estandarizado y propenso a errores de interpretación.

## 3. Usuarios Objetivo

- Equipo de desarrollo de MoLab.
- Ingenieros que ejecutan simulaciones de prueba y necesitan validar resultados rápidamente.

## 4. Requisitos Funcionales

### RF-01: Carga automática de datos

- El script debe cargar automáticamente el archivo CSV más reciente del directorio `output/`, o un archivo específico indicado por el usuario.
- Debe soportar el formato CSV actual con las columnas: `tick`, `simulation_time`, `utc_time`, `position_x/y/z`, `velocity_x/y/z`, `orientation_x/y/z/w`, `atm_density`, `atm_pressure`, `atm_temperature`, `gravity_x/y/z`, `wind_speed_x/y/z`.

### RF-02: Generación de gráficos clave

El script debe generar los siguientes gráficos:

| # | Gráfico                                | Descripción                                                      |
|---|----------------------------------------|------------------------------------------------------------------|
| 1 | **Trayectoria 3D**                     | Vista tridimensional de la trayectoria, coloreada por tiempo.    |
| 2 | **Altitud vs Tiempo**                  | Evolución de la altitud (position_z) a lo largo del tiempo.      |
| 3 | **Velocidad vs Tiempo**                | Magnitud del vector velocidad en función del tiempo.             |
| 4 | **Componentes de Velocidad vs Tiempo** | Vx, Vy, Vz superpuestas en un mismo gráfico.                    |
| 5 | **Condiciones Atmosféricas vs Tiempo** | Panel con 3 subgráficos: densidad, presión y temperatura.        |
| 6 | **Trayectoria 2D (Downrange vs Altitud)** | Perfil de vuelo clásico: distancia horizontal vs altitud.     |

### RF-03: Exportación de gráficos

- Los gráficos se guardan automáticamente como imágenes PNG en `output/plots/`.
- En modo interactivo se muestran en pantalla; con `--no-show` solo se guardan los archivos.

### RF-04: Integración con la interfaz web

- Al finalizar una simulación ejecutada desde la web (`molab_web_gui.py`), los gráficos se generan automáticamente.
- La interfaz web muestra una galería con los gráficos generados.
- Cada gráfico tiene un botón de descarga y se puede abrir a tamaño completo haciendo clic.

### RF-05: Modos de ejecución del script

- **Modo automático (por defecto):** Carga el CSV más reciente y genera todos los gráficos.
- **Modo archivo específico:** `--file <ruta>` para indicar un CSV concreto.
- **Modo sin ventana:** `--no-show` para generar solo los archivos PNG sin abrir ventanas.

## 5. Requisitos No Funcionales

| ID     | Requisito              | Detalle                                                                      |
|--------|------------------------|------------------------------------------------------------------------------|
| RNF-01 | **Compatibilidad**     | Python 3.8+ (consistente con el proyecto).                                   |
| RNF-02 | **Dependencias**       | `matplotlib` y `pandas` como dependencias externas.                          |
| RNF-03 | **Rendimiento**        | Procesar CSVs de hasta ~1.5 MB en menos de 10 segundos.                     |
| RNF-04 | **Portabilidad**       | Funcionar en Windows, Linux y macOS.                                         |
| RNF-05 | **Estilo de código**   | Seguir las convenciones existentes del proyecto.                             |
| RNF-06 | **Tolerancia a fallos** | Si la generación de plots falla, la simulación sigue marcada como exitosa.  |

## 6. Criterios de Aceptación

1. Existe un script `tools/visualize_results.py` en el repositorio que genera gráficos a partir de los CSV de simulación.
2. El script genera los 6 gráficos definidos en RF-02.
3. Los gráficos se guardan como PNG en `output/plots/`.
4. Al ejecutar una simulación desde la web, los gráficos se generan automáticamente al finalizar.
5. La interfaz web muestra los gráficos con opción de descarga.
6. El script se puede ejecutar de forma independiente desde la línea de comandos.

## 7. Entregables

| # | Entregable                              | Ubicación                    |
|---|-----------------------------------------|------------------------------|
| 1 | Script de visualización                 | `tools/visualize_results.py` |
| 2 | Integración en la interfaz web          | `tools/molab_web_gui.py`     |

## 8. Fuera de Alcance

- Gráficos interactivos (Plotly, Bokeh, etc.).
- Modificaciones al formato de salida CSV/JSON del simulador.
- Análisis estadístico avanzado (regresiones, FFT, etc.).
