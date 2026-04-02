# Pruebas unitarias

Este directorio contiene pruebas unitarias del proyecto, implementadas con Google Test.

## Objetivo

- Validar comportamiento del núcleo (`core`).
- Detectar regresiones funcionales en cambios de código.
- Facilitar validación automática en CI.

## Alcance

Actualmente existen pruebas para componentes como:

- `ConfigManager`
- `InitialStateLoader`
- `Logger`
- `OutputManager`
- `PhysicsIntegrator`
- `SimulationEngine`
- `TimeManager`

## Ejecución local

```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON

cmake --build build --parallel
cd build
ctest --output-on-failure --verbose
```

## Ejecución de un grupo específico

```bash
./bin/core_tests --gtest_filter=ConfigManagerTest.*
./bin/core_tests --gtest_filter=PhysicsIntegratorTest.*
./bin/core_tests --gtest_filter=TimeManagerTest.*
```

## Buenas prácticas

- Usar el patrón `Arrange-Act-Assert`.
- Mantener pruebas deterministas y aisladas.
- Evitar dependencias de estado compartido sin reinicio explícito.
- Preferir nombres de test descriptivos y orientados a comportamiento.

## Documentos relacionados

- `tests/TESTING_BEST_PRACTICES.md`
- `.github/workflows/` (ejecución automática en CI)
