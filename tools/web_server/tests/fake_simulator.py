#!/usr/bin/env python3
"""
Fake ``simulator`` used by the web-server tests.

It speaks the real ``--ipc stdio`` protocol (docs/IPC_PROTOCOL.md): ``initialize``,
``run_full``, ``run_ticks``, ``shutdown`` and ``get_status`` commands, with
``simulation_started`` / ``tick_completed`` / ``log`` / ``simulation_finished``
events. It never does physics. Instead it records what the web server handed to
it (config, initial state, working directory) in ``output/fake_run.json`` so tests
can assert that plugin toggles and gravity really reach the simulator.

Behavior switches (environment variables):
    FAKE_SIM_TICK_DELAY   seconds to sleep per tick (default 0)
    FAKE_SIM_FAIL_INIT    "1" to answer ``initialize`` with an error
    FAKE_SIM_FAIL_RUN     "1" to fail ``run_full`` like the real simulator does: an ``error``
                          event and ``simulation_finished`` with success=false, and NO ack
    FAKE_SIM_HANG         "1" to accept ``run_full`` and then never answer
"""
import json
import os
import sys
import time
from pathlib import Path


def emit(obj):
    sys.stdout.write(json.dumps(obj) + "\n")
    sys.stdout.flush()


CSV_HEADER = (
    "tick,simulation_time,utc_time,position_x,position_y,position_z,"
    "velocity_x,velocity_y,velocity_z,orientation_x,orientation_y,orientation_z,orientation_w,"
    "angular_velocity_x,angular_velocity_y,angular_velocity_z,total_mass,cg_x,cg_y,cg_z,"
    "mach_number,dynamic_pressure,angle_of_attack,sideslip_angle,"
    "atm_density,atm_pressure,atm_temperature,gravity_x,gravity_y,gravity_z,"
    "wind_velocity_x,wind_velocity_y,wind_velocity_z"
)


def write_csv(state, dt, rows=20):
    """Same columns as OutputManager; a drag-free ballistic arc under state['gravity']."""
    pos, vel = state.get("position", {}), state.get("velocity", {})
    g = state.get("gravity", {"x": 0.0, "y": 0.0, "z": 0.0})
    lines = [CSV_HEADER]
    for i in range(rows):
        t = i * dt
        p = [pos.get(a, 0.0) + vel.get(a, 0.0) * t + 0.5 * g.get(a, 0.0) * t * t for a in "xyz"]
        v = [vel.get(a, 0.0) + g.get(a, 0.0) * t for a in "xyz"]
        lines.append(",".join(str(x) for x in [
            i, t, 0.0, *p, *v, 0, 0, 0, 1, 0, 0, 0, state.get("total_mass", 1000.0), 0, 0, 0,
            0, 0, 0, 0, 1.225, 101325.0, 288.15, g.get("x", 0.0), g.get("y", 0.0), g.get("z", 0.0), 0, 0, 0]))
    return "\n".join(lines)


def main():
    args = sys.argv[1:]
    config_path = args[args.index("--config") + 1] if "--config" in args else None
    if "--ipc" not in args:
        sys.stderr.write("fake simulator only supports --ipc stdio\n")
        return 2

    config = json.loads(Path(config_path).read_text(encoding="utf-8"))
    state_path = config.get("initial_state_file")
    state = json.loads(Path(state_path).read_text(encoding="utf-8")) if state_path else {}

    out_dir = Path("output")
    out_dir.mkdir(exist_ok=True)

    seq = 0

    def event(name, payload=None):
        nonlocal seq
        seq += 1
        emit({"type": "event", "event": name, "event_seq": seq, "payload": payload or {}})

    delay = float(os.environ.get("FAKE_SIM_TICK_DELAY", "0"))
    max_iterations = int(config["simulation"]["max_iterations"])
    dt = float(config["simulation"].get("time_step", 0.1))
    tick_every = int(config.get("ipc", {}).get("tick_event_interval", 50))

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        cmd = json.loads(line)
        name = cmd.get("name") or cmd.get("command")
        rid = cmd.get("id") or cmd.get("request_id")

        if name == "initialize":
            if os.environ.get("FAKE_SIM_FAIL_INIT") == "1":
                emit({"type": "error", "ok": False, "request_id": rid,
                      "error": {"code": "engine_error", "message": "Failed to initialize simulation engine",
                                "details": {}}})
                continue
            event("log", {"level": "INFO", "component": "FakePlugin",
                          "message": "plugins: " + ",".join(
                              p["name"] for p in config.get("plugins", []) if p.get("enabled", True))})
            emit({"type": "ack", "ok": True, "request_id": rid, "data": {"initialized": True}})
        elif name in ("run_full", "run_ticks"):
            event("simulation_started", {"mode": name})
            if os.environ.get("FAKE_SIM_HANG") == "1":
                time.sleep(3600)
            if os.environ.get("FAKE_SIM_FAIL_RUN") == "1":
                event("error", {"message": "Integration diverged at tick 7"})
                event("simulation_finished", {"mode": name, "success": False, "tick": 7, "sim_time": 0.7})
                continue  # the real simulator sends no ack in this case
            for tick in range(1, max_iterations + 1):
                if delay:
                    time.sleep(delay)
                if tick % tick_every == 0:
                    event("tick_completed", {"tick": tick, "sim_time": tick * dt})
            event("log", {"level": "WARNING", "component": "FakePlugin", "message": "run finished"})
            event("simulation_finished", {"mode": name, "success": True, "tick": max_iterations})
            (out_dir / "fake_run.json").write_text(json.dumps({
                "cwd": os.getcwd(),
                "config": config,
                "initial_state": state,
            }, indent=2), encoding="utf-8")
            (out_dir / "fake_run.csv").write_text(write_csv(state, dt), encoding="utf-8")
            emit({"type": "ack", "ok": True, "request_id": rid, "data": {"tick": max_iterations}})
        elif name == "shutdown":
            emit({"type": "ack", "ok": True, "request_id": rid, "data": {"shutdown": True}})
        elif name == "get_status":
            emit({"type": "ack", "ok": True, "request_id": rid, "data": {"initialized": True}})
        else:
            emit({"type": "error", "ok": False, "request_id": rid,
                  "error": {"code": "unsupported_command", "message": f"Unsupported command: {name}",
                            "details": {}}})
    return 0


if __name__ == "__main__":
    sys.exit(main())
