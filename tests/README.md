# Unit tests

This directory contains project unit tests implemented with Google Test.

## Objective

- Validate core (`core`) behavior.
- Detect functional regressions across code changes.
- Support automated CI validation.

## Scope

Tests currently cover components such as:

- `ConfigManager`
- `InitialStateLoader`
- `Logger`
- `OutputManager`
- `PhysicsIntegrator`
- `SimulationEngine`
- `TimeManager`

## Local execution

```bash
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON

cmake --build build --parallel
cd build
ctest --output-on-failure --verbose
```

## Running a specific group

```bash
./bin/core_tests --gtest_filter=ConfigManagerTest.*
./bin/core_tests --gtest_filter=PhysicsIntegratorTest.*
./bin/core_tests --gtest_filter=TimeManagerTest.*
```

## Best practices

- Use the `Arrange-Act-Assert` pattern.
- Keep tests deterministic and isolated.
- Avoid shared-state dependencies without explicit reset.
- Prefer descriptive, behavior-oriented test names.

## Related documents

- `tests/TESTING_BEST_PRACTICES.md`
- `.github/workflows/` (automatic CI execution)
