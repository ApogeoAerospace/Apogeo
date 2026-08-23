# Apogeo Project Summary

## Overview

Apogeo is a modular aerospace simulation engine written in C++17. It models
the motion of aerospace vehicles by integrating rigid-body physics and running
an extensible pipeline of dynamic plugins. It supports command-line execution
and a JSON-lines IPC protocol over standard input and output.

The project is distributed under the Apache License 2.0. See the repository
`LICENSE` file for the complete terms.

## Goals

- Simulate position, velocity, attitude, angular velocity, forces, and torque
  on a tick-by-tick basis.
- Provide Euler, Runge-Kutta 4, and Verlet integration methods.
- Support dynamic plugins through a stable C API.
- Allow external control through the `--ipc stdio` protocol.
- Export simulation results in CSV, JSON, and binary formats.

## Technology Stack

| Component | Technology |
| --- | --- |
| Language | C++17 |
| Build system | CMake 3.20 or newer and Ninja |
| Dependency management | vcpkg |
| State serialization | FlatBuffers |
| Configuration and IPC | nlohmann/json |
| Linear algebra | Eigen 3.4 |
| ODE integration | Boost.Odeint |
| Tests | Google Test and CTest |

## Repository Layout

```text
Apogeo/
├── src/        # Core engine, C plugin API, IPC, and schemas
├── plugins/    # Dynamic plugin implementations and propulsion design
├── tests/      # Google Test suite
├── data/       # Default configuration, state, and model data
├── docs/       # Project and architecture documentation
├── scripts/    # CI and dependency scripts
└── tools/      # Auxiliary tools and legacy web tooling
```

## Core Components

### `SimulationEngine`

The main orchestrator combines simulation state, time, plugins, and output. It
initializes from `ConfigManager`, runs individual ticks or a full simulation,
and exposes read-only runtime status.

### `ConfigManager`

Loads and validates the simulation configuration, including duration, logging,
IPC settings, plugin definitions, initial-state location, and output location.

### `PluginManager`

Loads dynamic libraries and coordinates their lifecycle. Sequential plugins
(`type: 0`) may update the state buffer in a deterministic order. Parallel
physics plugins (`type: 1`) calculate forces and torques concurrently through
`PluginTaskScheduler`; their results are aggregated before physics integration.

### `PhysicsIntegrator`

Integrates the 13-component rigid-body ODE state: position, velocity,
orientation quaternion, and angular velocity. It supports `EULER`,
`RUNGE_KUTTA_4`, and `VERLET` methods and converts state to and from
FlatBuffers.

### `OutputManager`

Writes simulation data asynchronously in CSV, JSON, and binary formats. It can
also provide real-time telemetry through a configurable callback.

### `TimeManager`

Maintains relative simulation time and absolute UTC time with thread-safe
access.

### `InitialStateLoader`

Loads an initial JSON state and serializes it as `state_vector::GeneralState`
in the FlatBuffers state buffer.

### `Logger`

Provides console and file logging, a circular buffer for IPC replay, and a
structured logging callback used by `IpcSession`.

### `IpcSession` and `CommandEventProtocol`

Implement the `--ipc stdio` mode. Supported commands are `get_status`,
`initialize`, `run_ticks`, `run_full`, and `shutdown`. Events include logs,
errors, simulation lifecycle events, tick completion, and state samples.

## Active Plugin: `structures`

The `structures` plugin is the currently integrated plugin target. It loads
mass and inertia properties, reads structural limits, checks structural
integrity during runtime, and integrates with the host logger. Its force and
torque outputs are currently zero while structural physics remains pending.

## Plugin Contract

Every dynamic plugin exports the C functions declared in `src/api/plugin_api.h`.

| Function | Required | Responsibility |
| --- | --- | --- |
| `plugin_create_instance()` | Yes | Create and return an opaque handle. |
| `plugin_tick(handle, data*)` | Yes | Execute one simulation tick. |
| `plugin_destroy_instance(handle)` | Yes | Release the plugin instance. |
| `plugin_configure(handle, json*)` | No | Apply JSON configuration. |
| `plugin_set_host_services(services*)` | No | Receive host services such as logging. |

## Execution Flow

### CLI mode

```text
main.cpp
  -> load configuration and initialize logging
  -> SimulationEngine::initialize_from_loaded_config()
  -> repeat SimulationEngine::run_tick()
       -> run sequential and parallel plugins
       -> integrate physics
       -> update time and record output
```

### IPC mode

```text
main.cpp --ipc stdio
  -> IpcSession reads a JSON command
  -> SimulationEngine executes the requested operation
  -> IpcSession emits JSON acknowledgements and events
```

## Configuration

`data/defaults/default_config.json` contains the default simulation
configuration. It includes simulation parameters, plugin settings, initial
state and output paths, logging, IPC, performance, validation, vehicle models,
and mission-environment sections.

## Tests

The Google Test suite covers configuration, physics integration, time,
logging, output, simulation-engine lifecycle, initial-state loading, command
protocol parsing, and IPC sessions.

## Known Gaps

1. The configured output directory is not fully applied by the engine.
2. `simulation.enable_logging` is not a global logging switch.
3. Log rotation settings are not implemented.
4. Several advanced configuration sections are parsed but not yet integrated
   into runtime behavior.
5. The legacy web UI is deprecated in favor of `--ipc stdio`.

## Current Status

| Area | Status |
| --- | --- |
| Core simulation engine | Functional |
| Sequential and parallel plugin pipeline | Functional |
| Six-degree-of-freedom physics integration | Functional |
| CLI and IPC stdio modes | Supported |
| `structures` plugin | Active; structural force model pending |
| Output formats | CSV and JSON supported |
| Unit-test suite | Covers core managers |
| `propulsion` plugin | Architecture and interface-definition phase |
| Advanced configuration sections | Reserved or pending |
| Legacy web UI | Deprecated |
