# Compilación y ejecución (actual)

## Requisitos

- C++17
- CMake >= 3.20
- Soporte para Ninja
- FlatBuffers
- nlohmann_json
- Boost
- GTest (para pruebas)
- Eigen

## Compilación

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Ejecutar simulador

```bash
build/bin/simulator --config data/defaults/default_config.json
```

## Ejecutar pruebas

```bash
ctest --test-dir build --output-on-failure
```

## Notas del lanzador web

- `launch_web.sh` abre el navegador en `http://localhost:8080`
- `tools/molab_web_gui.py` actualmente escucha en el puerto `8082`

Esta diferencia debe resolverse en el roadmap.
