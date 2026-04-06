# MoLab CI Pipeline

This document summarizes the automated validation run in GitHub Actions.

## Objective

- Verify builds on supported platforms.
- Run unit tests.
- Apply quality checks and static analysis.
- Publish build and coverage artifacts.

## Main jobs

- `build-and-test`: multi-platform build and basic validation.
- `integration-test`: short simulation run with real configuration.
- `unit-tests`: test execution with Google Test.
- `code-coverage`: coverage generation with `lcov`/`genhtml`.
- `static-analysis`: static analysis with `clang-tidy`.
- `code-quality`: formatting and configuration-file validation.

## Relevant dependencies

- CMake (>= 3.20)
- Compilador C++17
- vcpkg for C++ dependencies
- Google Test
- Python 3 for utilities and scripts

## Equivalent local execution

### Build

```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --parallel
```

### Unit tests

```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON
cmake --build build --parallel
cd build
ctest --output-on-failure --verbose
```

### Coverage

```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON \
  -DENABLE_COVERAGE=ON
cmake --build build --parallel
cd build
ctest
```

## Artifacts

The pipeline publishes build artifacts, test reports, and coverage reports for later inspection.

## Documentation scope

This file is an operational summary. The exact pipeline definition lives in YAML files under `.github/workflows/`.
