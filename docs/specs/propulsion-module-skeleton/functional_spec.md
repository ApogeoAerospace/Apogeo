# Propulsion module skeleton — functional specification

Status: Current — implemented and verified; see task_list.md.

## Objective and scope

Initialize an engine from a validated CSV performance table, exposing its operating
parameters and specific impulse in memory. Isolate data ingestion and interpolation
from the future thermodynamic solver. Deliver the active module under
`plugins/propulsion`, a synthetic dataset and a compilable initialization test.

The available propulsion definition is `plugins/propulsion/data/VALIDATION_MERLIN.md`.
It supplies reference scenarios, not a complete CSV schema or engine lifecycle.
The decisions below complete that missing contract for this increment. The mock is
synthetic and is not a validated Merlin model; the existing reference stays intact.

## Use cases

1. A developer loads an engine CSV: the complete table and fixed chamber pressure /
   oxidizer-to-fuel mass ratio become available through read-only accessors.
2. A developer requests tabulated performance at an ambient pressure: exact nodes
   and linear interpolation inside the data domain return thrust and ISP.
3. A malformed file fails with its path, physical line and reason. No partial data
   become visible. A failed reload preserves the last successful dataset.
4. The host creates a plugin without requiring files yet, then configures its CSV
   path. Successful configuration makes the instance ready for neutral ticks.
5. A tick on a configured instance validates the state buffer and returns zero
   force / torque without modifying state, fuel mass or simulation time.
6. Physical thrust and mass-flow methods explicitly return “not implemented” via
   `std::nullopt`, rather than pretending the tabulated curve is a cycle solver.

## Acceptance mapping

| Requirement | Deliverable | Evidence |
| --- | --- | --- |
| Structural source integrated in C++ architecture | Domain module, CSV reader, C ABI adapter, root CMake target | Build and plugin lifecycle test |
| Standard CSV columns | Version 1 schema in technical_spec.md | Mock and parser schema tests |
| Parse data into memory | Strict reader with atomic module loading | Valid/invalid file and reload tests |
| ISP storage, thrust/mass-flow placeholders | Typed rows and explicit optional calculation methods | Data assertions and pending-physics assertions |
| Compilable initialization test | `plugins/propulsion/tests/test_propulsion.cpp` | GTest / CTest |

## Boundaries

One steady-state pressure curve per instance, at fixed chamber pressure and mixture
ratio. No database backend, multidimensional interpolation, combustion solution,
ignition/shutdown dynamics, throttle control, fuel depletion, gimbal or mass mutation.
No change to the host ABI or FlatBuffers schema. Propulsion remains disabled in the
default simulation configuration until physical behavior is implemented.
