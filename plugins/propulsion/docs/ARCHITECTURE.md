# Propulsion plugin architecture

## 1. Purpose

This design defines the internal structure of the MoLab propulsion plugin. It
supports multiple engines and tanks while keeping engine physics, propellant
consumption, host communication, and configuration independent.

The design has two public boundaries:

1. The stable C ABI consumed by `MoLab::PluginManager`.
2. The typed C++ `IPropulsionModule` contract used inside the plugin.

The C boundary preserves dynamic loading across compilers and platforms. The C++
boundary provides the object-oriented extension point for propulsion behavior.

## 2. Repository constraints considered

- The host loads plugins dynamically and resolves the functions declared in
  `src/api/plugin_api.h`.
- Tick state is serialized as `state_vector::GeneralState` using FlatBuffers.
- Engine commands currently arrive in `GeneralState.engines` by vector index and
  contain throttle and TVC angles.
- A type-1 plugin runs in the parallel physics phase and returns force and torque.
- Type-1 plugins share the same state buffer, so they must treat it as read-only.
- The physics integrator currently consumes translational force as an inertial
  vector and rotational torque with the body-frame inertia tensor.

Propulsion should therefore be configured as
`PARALLEL_PHYSICS_CALCULATOR` (`type: 1`) in the current host.

## 3. Proposed structure

```text
plugins/propulsion/
├── data/
├── docs/
│   ├── ARCHITECTURE.md
│   ├── propulsion_class_diagram.puml
│   └── propulsion_class_diagram.png
├── include/propulsion/
│   ├── interfaces.hpp
│   ├── models.hpp
│   ├── plugin_adapter.hpp
│   ├── propulsion_module.hpp
│   └── types.hpp
└── README.md
```

Future implementation files should mirror the include structure under `src/`,
with the exported functions in a small `propulsion.cpp` composition root.

## 4. Public host contract

The principal contract with the orchestrator is the existing C ABI. It is
redeclared in `plugin_adapter.hpp` to make the propulsion boundary explicit.

| Function | Parameters | Result and responsibility |
| --- | --- | --- |
| `plugin_create_instance()` | None | Returns an opaque `PluginHandle`, or `nullptr` on failure. Builds the adapter and default dependencies. |
| `plugin_configure(handle, json_params)` | Plugin handle and UTF-8 JSON string | Returns `0` on success or a negative `ErrorCode`. Parses and validates engines, tanks, data sources, and policies. |
| `plugin_tick(handle, data)` | Plugin handle and mutable `PluginTickData*` | Reads the FlatBuffer and commands, executes one deterministic propulsion step, and writes force/torque outputs. |
| `plugin_destroy_instance(handle)` | Plugin handle | Releases the adapter and all owned models. Must accept `nullptr`. |
| `plugin_set_host_services(services)` | Optional versioned host services | Installs the host logging callback when API version 1 is supported; otherwise keeps the local fallback. |

`plugin_tick` must validate `handle`, `data`, `state_buffer`, `buffer_size`,
`delta_time`, FlatBuffer integrity, command count, and output pointers before
invoking the domain.

## 5. Internal typed contract

`IPropulsionModule` is the main C++ application contract:

```cpp
virtual Status configure(const PropulsionConfig& configuration) = 0;
virtual Status tick(const TickInput& input, TickResult& output) = 0;
virtual void reset() noexcept = 0;
virtual PropulsionSnapshot snapshot() const = 0;
```

The interface has no dependency on JSON, FlatBuffers, dynamic loading, or the
host logger. It can therefore be unit-tested with plain C++ values.

## 6. Responsibilities

### `PluginAdapter`

- Implements the behavior behind the five C exports.
- Converts exceptions into stable integer error codes.
- Owns one `IPropulsionModule` and one `IStateMapper`.
- Uses `PluginHostServices` without depending on the core `Logger`.
- Never lets C++ exceptions cross the C ABI.

### `FlatBufferStateMapper`

- Verifies and reads `GeneralState`.
- Maps atmosphere, vehicle attitude, throttle, and TVC commands into `TickInput`.
- Converts inertial force and body torque into `PluginVector3`.
- Does not own propulsion state or apply engine physics.

### `PropulsionModule`

- Validates tick invariants.
- Evaluates all engines in deterministic configuration order.
- Sends all engine demands to the propellant system in one allocation operation.
- Resolves starvation/partial delivery through each engine model.
- Computes torque with `r × F` at each engine mounting point.
- Rotates aggregate body force into the inertial frame.
- Returns engine and tank telemetry in `TickResult`.

### `IEngineModel`

- Encapsulates engine-specific performance and lifecycle.
- Produces ideal thrust and propellant demand from command and environment.
- Resolves actual output after propellant allocation.
- Supports polymorphic liquid, solid, and tabulated models.

The two-phase `evaluate`/`resolve` contract avoids order-dependent tank depletion:
all demands are known before the tank system commits consumption.

### `IPropellantSystem`

- Owns tank masses and enforces non-negative remaining mass.
- Allocates propellant atomically across engines for a complete tick.
- Returns a fulfillment ratio and delivered flow for each engine.
- Can later be replaced by a network/feed-system model without changing engines
  or the host adapter.

### `EngineRegistry`

- Maps `model_type` strings to constructor functions.
- Allows a new engine model to be registered without modifying
  `PropulsionModule`.
- Centralizes unknown-model validation.

## 7. Tick sequence

1. `PluginManager` invokes `plugin_tick`.
2. `PluginAdapter` validates the C arguments.
3. `FlatBufferStateMapper` verifies and maps the shared state as read-only.
4. `PropulsionModule` asks each engine to evaluate its command.
5. `IPropellantSystem` allocates all requested propellants atomically.
6. Each engine resolves actual thrust from its allocation.
7. The module aggregates body-frame forces and torques.
8. The module rotates force to the inertial frame.
9. `FlatBufferStateMapper` writes only `output_force` and `output_torque`.
10. The host aggregates plugin outputs and performs physics integration.

## 8. Object-oriented design

- **Encapsulation:** tank state stays in `IPropellantSystem`; engine lifecycle
  stays in each `IEngineModel`.
- **Abstraction:** host communication depends on `IPropulsionModule`, not on a
  specific engine.
- **Inheritance and polymorphism:** `LiquidRocketEngine`,
  `SolidRocketEngine`, and `TabulatedEngineModel` implement `IEngineModel`.
- **Single Responsibility:** mapping, orchestration, engine physics, tank
  allocation, and construction have separate classes.
- **Open/Closed:** new models are added through `EngineRegistry`.
- **Dependency Inversion:** `PropulsionModule` receives `IEngineFactory` and
  `IPropellantSystem` abstractions.
- **Composition over inheritance:** an engine assembly combines a model,
  mounting geometry, command, and tank allocation; model inheritance is limited
  to the behavior extension point.

## 9. Configuration proposal

The plugin parameters should own propulsion runtime configuration. The existing
`vehicle_models.propulsion_model.source` can remain the default data-source
reference.

```json
{
  "use_host_logger": true,
  "strict_command_count": true,
  "tanks": [
    {
      "id": "lox-main",
      "propellant_type": "LOX",
      "initial_mass_kg": 10000.0,
      "capacity_kg": 10000.0
    },
    {
      "id": "rp1-main",
      "propellant_type": "RP-1",
      "initial_mass_kg": 3906.25,
      "capacity_kg": 3906.25
    }
  ],
  "engines": [
    {
      "id": "merlin-1",
      "model_type": "liquid_rocket",
      "mounting_position_body_m": {"x": -6.0, "y": 0.0, "z": 0.0},
      "nominal_axis_body": {"x": 1.0, "y": 0.0, "z": 0.0},
      "parameters": {
        "sea_level_thrust_n": 845000.0,
        "vacuum_isp_s": 311.0,
        "sea_level_isp_s": 282.0,
        "mixture_ratio": 2.56,
        "fuel_tank_id": "rp1-main",
        "oxidizer_tank_id": "lox-main"
      }
    }
  ]
}
```

Configuration validation should reject duplicate IDs, missing tank references,
negative masses, zero-length axes, invalid mixture ratios, throttle limits
outside `[0, 1]`, and unsupported model types.

## 10. Error handling and invariants

- C exports return stable negative codes and never throw.
- Domain methods return `Status`; predictable simulation states are not modeled
  as exceptions.
- Outputs are initialized to zero before any operation that may fail.
- `delta_time_s` must be finite and greater than zero.
- Quaternion and vector inputs must be finite.
- Tank mass never becomes negative.
- Throttle is validated or clamped according to configuration policy.
- Engine-to-command mapping is deterministic and configuration-ordered.
- A failed engine contributes zero force and an engine-level diagnostic.

## 11. Host contract gap and recommended evolution

The current plugin ABI cannot safely perform both propulsion responsibilities in
one type-1 tick:

- returning force/torque; and
- committing reduced propellant and total vehicle mass.

Mutating `state_buffer` from a parallel plugin would race with other type-1
plugins. Keeping fuel only inside the plugin avoids the race but leaves
`GeneralState.total_mass` and `propellant_masses` stale.

Recommended host evolution:

1. Add a versioned `PluginTickOutputV2` containing force, torque, total mass
   delta, and indexed propellant mass deltas.
2. Let `PluginManager` gather all outputs in parallel and commit state changes
   once, before physics integration.
3. Add `plugin_get_api_version()` and capability flags so the host can negotiate
   outputs rather than infer behavior from plugin type.
4. Replace index-only engine commands with stable engine IDs and explicit
   ignition/shutdown fields.
5. Document coordinate frames in `plugin_api.h`; this design assumes inertial
   force and body torque to match the current integrator.

Until that evolution lands, the adapter should keep the FlatBuffer read-only,
retain tank state internally, return force/torque, and expose remaining fuel only
through diagnostics or future telemetry. This is compatible but should be
treated as an interim limitation, not the final mass model.

## 12. Suggested implementation order

1. Implement and unit-test math/value validation and `EngineRegistry`.
2. Implement `TankPropellantSystem` with multi-engine starvation tests.
3. Implement a `TabulatedEngineModel` against the configured CSV source.
4. Implement the Merlin liquid model using the existing validation document.
5. Implement `PropulsionModule` aggregation, TVC, torque, and frame rotation.
6. Implement `FlatBufferStateMapper` and the C adapter.
7. Add the plugin target to root CMake and configure it as type 1.
8. Evolve the host output contract before enabling authoritative mass depletion.

## 13. Acceptance-criteria traceability

| Criterion | Evidence |
| --- | --- |
| Editable UML class diagram | `docs/propulsion_class_diagram.puml` |
| Rendered image | `docs/propulsion_class_diagram.png` |
| Explicit orchestrator contract | UML `PropulsionPluginAPI` plus section 4 |
| Inheritance and polymorphism | `IEngineModel` hierarchy in UML and `models.hpp` |
| Initial C/C++ headers | `include/propulsion/*.hpp` |
| Extensible engine integration | `IEngineFactory` and `EngineRegistry` |
