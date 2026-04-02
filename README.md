# Simulador Aeroespacial MoLab
# MoLab Aerospace Simulator

Motor de simulación aeroespacial modular con arquitectura de plugins e integración de fuerzas en tiempo real.

## Requisitos

- Compilador compatible con C++17
- CMake 3.20 o superior
- Python 3 (interfaces y utilidades)

## Compilación rápida

```bash
cmake -B build -S .
cmake --build build --parallel
```

## Ejecución

### Interfaz web (recomendada)

```bash
./launch_web.sh
```

Abrir `http://localhost:8080`.

### Lanzador inteligente

```bash
./launch_molab.sh
```

### GUI de escritorio

```bash
./launch_gui.sh
```

### Línea de comandos

```bash
./bin/simulator --config ../data/config/basic_config.json --ticks 50
./bin/simulator --config ../data/config/main_config.json --ticks 100
```

## Documentación

- Índice de documentación del proyecto: `docs/README.md`
- Guía de estilo Doxygen: `docs/DOCUMENTATION_STYLE_GUIDE.md`
- Salida HTML generada por Doxygen: `docs/generated/html/index.html`

## Estructura del repositorio

```text
MoLab/
├── src/                  # Código fuente principal
├── tests/                # Pruebas unitarias (Google Test)
├── docs/                 # Documentación técnica y funcional
├── tools/                # Utilidades y servidores auxiliares
├── data/                 # Configuración y estados iniciales
└── plugins/              # Plugins de simulación
```

## Licencia

Proyecto bajo licencia MIT.
