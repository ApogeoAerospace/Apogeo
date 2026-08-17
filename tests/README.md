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
- `CommandEventProtocol`
- `IpcSession`

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
./bin/core_tests --gtest_filter=CommandEventProtocolTest.*
./bin/core_tests --gtest_filter=IpcSessionTest.*
```

## Test inventory

- `test_config_manager.cpp`: configuration loading, defaults, and validation.
- `test_initial_state_loader.cpp`: state JSON parsing into FlatBuffers.
- `test_logger.cpp`: log levels, formatting, append behavior, and thread safety.
- `test_output_manager.cpp`: output persistence and generated file behavior.
- `test_physics_integrator.cpp`: integration methods, analytical checks, and edge cases.
- `test_simulation_engine.cpp`: engine lifecycle and integration flow behavior.
- `test_time_manager.cpp`: simulation time, UTC conversion, and reset semantics.
- `test_command_event_protocol.cpp`: command parsing and ACK/error/event message builders.
- `test_ipc_session.cpp`: throttle defaults, payload overrides, and validation failures.

## Best practices

- Use the `Arrange-Act-Assert` pattern.
- Keep tests deterministic and isolated.
- Avoid shared-state dependencies without explicit reset.
- Prefer descriptive, behavior-oriented test names.

## Related documents

- `tests/TESTING_BEST_PRACTICES.md`
- `.github/workflows/` (automatic CI execution)
