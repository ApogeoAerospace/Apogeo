# Propulsion module

Status: Current — data-loading skeleton; physical engine-cycle calculations pending.

This module builds on the architecture and interface contracts already defined for
propulsion. The current implementation follows the Structures split between domain
code and the C ABI adapter. It loads a strict numeric CSV into memory and interpolates
tabulated thrust / ISP. The thrust and mass-flow solver methods return `std::nullopt`.
Runtime ticks emit zero force/torque and leave the FlatBuffers state untouched.

## Architecture deliverables

- [Architecture](docs/ARCHITECTURE.md)
- [Editable UML source](docs/propulsion_class_diagram.puml)
- [Rendered UML diagram](docs/propulsion_class_diagram.png)
- [Initial C++ interfaces](include/propulsion/interfaces.hpp)
- [Initial model hierarchy](include/propulsion/models.hpp)
- [Host boundary adapter](include/propulsion/plugin_adapter.hpp)

## Build and test

Use the repository's root build with its configured dependency toolchain:

```bash
cmake -S . -B build
cmake --build build --target propulsion propulsion_tests
ctest --test-dir build -R 'Propulsion' --output-on-failure
```

The shared plugin is placed under `build/bin/plugins`; the mock is copied to
`build/bin/data/propulsion/engine_curves.csv`. Tests live in this module's `tests/`
directory and run through GTest/CTest. There is no separate standalone CMake project.

## Configure

The default simulator configuration includes this plugin disabled. To exercise the
skeleton, enable that entry and run from `build/bin` using the copied configuration.
It has the following shape:

```json
{
  "name": "propulsion",
  "type": 1,
  "library_path": "plugins/propulsion",
  "enabled": true,
  "parameters": {
    "engine_curves_path": "data/propulsion/engine_curves.csv"
  }
}
```

Relative dataset paths resolve only against the working directory. Absolute paths
are also accepted. `vehicle_models.propulsion_model.source` is not routed to the
plugin by the current host; use the parameter above. Creation performs no file I/O;
configuration must succeed before a tick. Explicit `{}` passed to `plugin_configure`
loads the default CSV, but the host skips configure for absent/empty parameters.
CSV diagnostics are emitted to stderr; this skeleton does not export host logging.

## Data and API

`data/engine_curves.csv` is synthetic test data, **not validated Merlin performance**.
The table has one ambient-pressure axis, fixed chamber pressure and fixed mixture
ratio. SI units are encoded in column names. Only the compiled `linear` lookup is
supported; out-of-range lookups return empty. Failed loads preserve prior data.

```cpp
propulsion::PropulsionModule engine;
engine.load_from_csv("data/propulsion/engine_curves.csv");
auto tabulated = engine.interpolate_performance(25000.0); // 107500 N, 295 s
const auto& samples = engine.curve().samples;
auto physical_thrust = engine.compute_thrust_n({25000.0, 1.0}); // nullopt for now
```

The original [Merlin reference scenarios](data/VALIDATION_MERLIN.md) remain future
physical validation inputs; the current tests establish software behavior only.

See the [functional specification](../../docs/specs/propulsion-module-skeleton/functional_spec.md),
[CSV and technical contract](../../docs/specs/propulsion-module-skeleton/technical_spec.md),
and [work plan / verification record](../../docs/specs/propulsion-module-skeleton/task_list.md).
