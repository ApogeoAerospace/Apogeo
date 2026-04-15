#!/bin/bash
#!/usr/bin/env bash

# MoLab Dependencies Checker (cross-platform guidance)

set -euo pipefail

echo "🔧 MoLab dependency check"

command_exists() {
    command -v "$1" >/dev/null 2>&1
}

OS="$(uname -s)"
case "${OS}" in
    Linux*) MACHINE="Linux" ;;
    Darwin*) MACHINE="macOS" ;;
    CYGWIN*|MINGW*|MSYS*) MACHINE="Windows" ;;
    *) MACHINE="Unknown" ;;
esac

echo "📋 Detected platform: ${MACHINE}"
echo

print_install_help() {
    local package="$1"
    echo "   Install ${package}:"
    if [ "${MACHINE}" = "macOS" ]; then
        echo "   - brew install ${package}"
    elif [ "${MACHINE}" = "Linux" ]; then
        echo "   - Ubuntu/Debian: sudo apt-get install ${package}"
        echo "   - Fedora: sudo dnf install ${package}"
    elif [ "${MACHINE}" = "Windows" ]; then
        echo "   - winget install Kitware.CMake / Python.Python.3 / Git.Git"
        echo "   - or choco install cmake python git ninja"
    fi
}

echo "🔍 CMake"
if command_exists cmake; then
    echo "✅ $(cmake --version | head -n1)"
else
    echo "❌ CMake not found"
    print_install_help "cmake"
fi

echo
echo "🔍 Ninja"
if command_exists ninja; then
    echo "✅ $(ninja --version)"
else
    echo "⚠️  Ninja not found (recommended generator)"
    print_install_help "ninja"
fi

echo
echo "🔍 Python 3"
if command_exists python3; then
    echo "✅ $(python3 --version)"
elif command_exists python; then
    echo "✅ $(python --version)"
elif command_exists py; then
    echo "✅ $(py -3 --version)"
else
    echo "❌ Python 3 not found"
    print_install_help "python"
fi

echo
echo "🔍 C++ compiler"
if command_exists g++; then
    echo "✅ $(g++ --version | head -n1)"
elif command_exists clang++; then
    echo "✅ $(clang++ --version | head -n1)"
elif command_exists cl; then
    echo "✅ MSVC cl.exe detected"
else
    echo "❌ No C++ compiler found"
    if [ "${MACHINE}" = "macOS" ]; then
        echo "   - xcode-select --install"
    elif [ "${MACHINE}" = "Linux" ]; then
        echo "   - Ubuntu/Debian: sudo apt-get install build-essential"
        echo "   - Fedora: sudo dnf groupinstall 'Development Tools'"
    elif [ "${MACHINE}" = "Windows" ]; then
        echo "   - Install Visual Studio Build Tools (Desktop development with C++)"
    fi
fi

echo
echo "🔍 Git"
if command_exists git; then
    echo "✅ $(git --version)"
else
    echo "❌ Git not found"
    print_install_help "git"
fi

echo
echo "✅ Dependency check finished"
echo "Next steps:"
echo "1) Configure: cmake -S . -B build -G Ninja"
echo "2) Build:     cmake --build build --parallel"
echo "3) Launch UI: ./scripts/launch_web.sh (or scripts/launch_web.ps1 on Windows)"
