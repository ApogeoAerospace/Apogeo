# Pipeline de CI de MoLab

Este documento resume la validación automática ejecutada en GitHub Actions.

## Objetivo

- Verificar compilación en plataformas soportadas.
- Ejecutar pruebas unitarias.
- Aplicar controles de calidad y análisis estático.
- Publicar artefactos de build y cobertura.

## Jobs principales

- `build-and-test`: compilación y validación básica multi-plataforma.
- `integration-test`: ejecución de simulación corta con configuración real.
- `unit-tests`: ejecución de pruebas con Google Test.
- `code-coverage`: generación de cobertura con `lcov`/`genhtml`.
- `static-analysis`: análisis estático con `clang-tidy`.
- `code-quality`: validación de formato y archivos de configuración.

## Dependencias relevantes

- CMake (>= 3.20)
- Compilador C++17
- vcpkg para dependencias C++
- Google Test
- Python 3 para utilidades y scripts

## Ejecución local equivalente

### Compilación

```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --parallel
```

### Pruebas unitarias

```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON
cmake --build build --parallel
cd build
ctest --output-on-failure --verbose
```

### Cobertura

```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON \
  -DENABLE_COVERAGE=ON
cmake --build build --parallel
cd build
ctest
```

## Artefactos

El pipeline publica artefactos de build, reportes de tests y cobertura para inspección posterior.

## Alcance documental

Este archivo es un resumen operativo. La definición exacta del pipeline está en los archivos YAML de `.github/workflows/`.
