#!/bin/bash
#!/usr/bin/env bash

# MoLab Web Interface Launcher (cross-platform)

set -euo pipefail

cd "$(dirname "$0")/.."

find_python() {
    if command -v python3 >/dev/null 2>&1; then
        echo "python3"
    elif command -v python >/dev/null 2>&1; then
        echo "python"
    elif command -v py >/dev/null 2>&1; then
        echo "py -3"
    else
        return 1
    fi
}

PYTHON_CMD="$(find_python || true)"
if [ -z "${PYTHON_CMD}" ]; then
    echo "❌ Python 3 not found."
    exit 1
fi

if [ ! -d "build" ]; then
    echo "⚠️  Build directory not found. Configuring project..."
    cmake -S . -B build -G Ninja
fi

SIMULATOR="build/bin/simulator"
case "$(uname -s)" in
    CYGWIN*|MINGW*|MSYS*) SIMULATOR="build/bin/simulator.exe" ;;
esac

if [ ! -f "${SIMULATOR}" ]; then
    echo "⚠️  Simulator not found. Building project..."
    cmake -S . -B build -G Ninja
    cmake --build build --parallel
fi

if [ ! -f "${SIMULATOR}" ]; then
    echo "❌ Could not find simulator binary after build: ${SIMULATOR}"
    exit 1
fi

echo "🌐 Starting MoLab web interface"
echo "   URL: http://localhost:8082"
echo "   Stop with Ctrl+C"

exec ${PYTHON_CMD} tools/molab_web_gui_v2.py
