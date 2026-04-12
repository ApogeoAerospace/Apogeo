# Apogeo Aerospace Simulator

Modular aerospace simulation engine with plugin architecture, FlatBuffers state exchange, and real-time force/torque integration.

## What Apogeo includes
- Core simulation runtime in C++17 (`simulator`)
- Plugin execution pipeline (sequential + parallel plugin roles)
- Physics integration (`Euler`, `Runge-Kutta 4`, `Verlet`)
- Stable CLI execution path for simulation runs
- Unit tests with Google Test + CTest

## Current repository status

- Build system: CMake (minimum `3.20`)
- Recommended generator: `Ninja`
- Active plugin configured in build: `structures`
- Additional legacy/deprecated plugins and docs are kept for historical traceability

## Requirements

- C++17-compatible compiler
- CMake `>= 3.20`
- Python 3
- Ninja (recommended)

Quick dependency check:

- macOS/Linux: `./scripts/install_dependencies.sh`
- Windows PowerShell: `./scripts/install_dependencies.ps1`

## Build

### Recommended (Ninja)

```bash
cmake -S . -B build -G Ninja
cmake --build build --parallel
```

### With tests explicitly enabled

```bash
cmake -S . -B build -G Ninja -DENABLE_TESTING=ON
cmake --build build --parallel
```

## Run

### Current supported execution path

At the moment, visual interfaces are not considered operational.
Use the core simulator directly through CLI.

### CLI simulation

```bash
build/bin/simulator --config data/defaults/default_config.json --ticks 50
```

On Windows this may be:

```powershell
build/bin/simulator.exe --config data/defaults/default_config.json --ticks 50
```

### IPC stdio mode (skeleton)

`simulator` supports an optional JSON-line IPC mode:

```bash
echo '{"type":"command","id":"1","name":"get_status"}' | build/bin/simulator --ipc stdio
```

Current supported commands:

- `get_status`
- `initialize`
- `run_ticks` (payload: `{ "count": <positive-int> }`)
- `run_full`
- `shutdown`

See `docs/IPC_PROTOCOL.md` for request/response/event examples.

Important IPC semantics are also documented there:

- `shutdown` releases engine resources but does not, by itself, terminate the `--ipc stdio` process.
- For compatibility, command parsing accepts both `name`/`command` and `id`/`request_id`.
- On IPC session start, recent logger history is replayed as IPC events (capped to `256` entries).
- In IPC mode, plain console log lines are disabled to keep stdout JSON-only.

## Scripts (core workflow)

- Dependency check:
  - macOS/Linux: `./scripts/install_dependencies.sh`
  - Windows PowerShell: `./scripts/install_dependencies.ps1`

- Local CI-like smoke checks:
  - macOS/Linux: `./scripts/test_ci_pipeline.sh`
  - Windows PowerShell: `./scripts/test_ci_pipeline.ps1`

> Note: Web-launch scripts are present in the repository, but they are currently not part of the supported workflow.

## Test

```bash
ctest --test-dir build --output-on-failure
```

Main test coverage includes:

- `ConfigManager`
- `PhysicsIntegrator`
- `TimeManager`
- `SimulationEngine`
- `OutputManager`
- `InitialStateLoader`

## Documentation

- Documentation index: `docs/README.md`
- Architecture: `docs/ARCHITECTURE.md`
- Build and run guide: `docs/BUILD_RUN.md`
- Configuration guide: `docs/CONFIG.md`
- Full config reference: `docs/CONFIG_REFERENCE.md`
- IPC protocol: `docs/IPC_PROTOCOL.md`
- Plugin API: `docs/PLUGIN_API.md`
- Structures module implementation: `docs/STRUCTURES_MODULE.md`
- Known gaps: `docs/KNOWN_GAPS.md`
- Doxygen style guide: `docs/DOCUMENTATION_STYLE_GUIDE.md`
- Doxygen generated HTML: `docs/generated/html/index.html`

## Repository structure

```text
MoLab/
├── src/                  # Core engine, API headers, schema
├── plugins/              # Runtime plugins (active + deprecated)
├── tests/                # Google Test-based unit tests
├── tools/                # Web server and helper tooling
├── scripts/              # Cross-platform helper scripts
├── data/                 # Default config/state and model data
├── docs/                 # Active, generated, and historical docs
└── CMakeLists.txt        # Main build entry
```

## License

GNU Affero General Public License v3.0 (AGPL-3.0)
