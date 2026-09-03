#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
from typing import Optional
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]


def default_simulator_path() -> Path:
    exe_name = "simulator.exe" if os.name == "nt" else "simulator"

    candidates = [
        PROJECT_ROOT / "build" / "x64-Debug" / "bin" / exe_name,
        PROJECT_ROOT / "build" / "x64-Release" / "bin" / exe_name,
    ]

    for path in candidates:
        if path.exists():
            return path

    return candidates[0]


def send_command(proc: subprocess.Popen, request_id: str, name: str, payload: Optional[dict] = None) -> dict:
    command = {
        "type": "command",
        "id": request_id,
        "name": name,
        "payload": payload or {},
    }

    line = json.dumps(command)
    print(f">> {line}")
    proc.stdin.write(line + "\n")
    proc.stdin.flush()

    while True:
        response_line = proc.stdout.readline()
        if not response_line:
            raise RuntimeError("Simulator exited or closed stdout before replying")

        response_line = response_line.strip()
        if not response_line:
            continue

        print(f"<< {response_line}")

        try:
            message = json.loads(response_line)
        except json.JSONDecodeError:
            continue

        if message.get("type") in {"ack", "error"} and message.get("request_id") == request_id:
            return message


def main() -> int:
    parser = argparse.ArgumentParser(description="Simple IPC controller for the Apogeo simulator")
    parser.add_argument("--simulator", type=Path, default=default_simulator_path(), help="Path to simulator executable")
    parser.add_argument("--config", type=Path, default=PROJECT_ROOT / "data" / "defaults" / "default_config.json", help="Path to config file")
    parser.add_argument("--ticks", type=int, default=10, help="Number of ticks to run")
    args = parser.parse_args()

    if args.ticks <= 0:
        print("--ticks must be > 0", file=sys.stderr)
        return 2

    if not args.simulator.exists():
        print(f"Simulator not found: {args.simulator}", file=sys.stderr)
        return 2

    if not args.config.exists():
        print(f"Config not found: {args.config}", file=sys.stderr)
        return 2

    cmd = [
        str(args.simulator),
        "--config",
        str(args.config),
        "--ipc",
        "stdio",
    ]

    print("Starting simulator:", " ".join(cmd))

    proc = subprocess.Popen(
        cmd,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1,
        cwd=str(PROJECT_ROOT),
    )

    try:
        init_response = send_command(proc, "1", "initialize")
        send_command(proc, "2", "get_status")
        if init_response.get("type") == "ack":
            send_command(proc, "3", "run_ticks", {"count": args.ticks})
            send_command(proc, "4", "get_status")
        else:
            print("Initialization failed; skipping run_ticks.", file=sys.stderr)
        send_command(proc, "5", "shutdown")
    finally:
        if proc.stdin:
            proc.stdin.close()

        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.terminate()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proc.kill()

        stderr_output = ""
        if proc.stderr:
            stderr_output = proc.stderr.read().strip()

        if stderr_output:
            print("Simulator stderr:")
            print(stderr_output)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
