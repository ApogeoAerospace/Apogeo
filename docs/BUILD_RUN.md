# Build and Run (Current)

## Requirements

- C++17
- CMake >= 3.20
- Ninja support
- FlatBuffers
- nlohmann_json
- Boost
- GTest (for tests)
- Eigen

## Build

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Run simulator

```bash
build/bin/simulator --config data/defaults/default_config.json
```

## Run simulator in IPC stdio mode

```bash
echo '{"type":"command","id":"1","name":"get_status"}' | build/bin/simulator --ipc stdio
```

Quick command examples:

```bash
echo '{"type":"command","id":"1","name":"initialize"}' | build/bin/simulator --ipc stdio
echo '{"type":"command","id":"2","name":"run_ticks","payload":{"count":10}}' | build/bin/simulator --ipc stdio
```

See `docs/IPC_PROTOCOL.md` for the full IPC contract.

Protocol notes:

- `shutdown` finalizes engine resources but does not automatically exit the process in `--ipc stdio` mode.
- `run_ticks` returns an `ack` on success; if execution fails mid-run, an `error` is emitted instead of a success `ack`.

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

## Web launcher notes

- `scripts/launch_web.sh` opens the browser on `http://localhost:8082`
- `tools/molab_web_gui_v2.py` is the current launcher entry point

This behavior is now aligned with the current scripts.
