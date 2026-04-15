#!/usr/bin/env bash
#!/usr/bin/env bash

# Local CI smoke pipeline (Linux/macOS/Git-Bash)

set -euo pipefail

BLUE='\033[0;34m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

print_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}"
}

print_ok() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warn() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

cd "$(dirname "$0")/.."

BUILD_DIR="build"
TOOLCHAIN_FILE="$(pwd)/vcpkg/scripts/buildsystems/vcpkg.cmake"
CMAKE_ARGS=( -B "${BUILD_DIR}" -S . -G Ninja )

if [ -f "${TOOLCHAIN_FILE}" ]; then
    CMAKE_ARGS+=( -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" )
else
    print_warn "vcpkg toolchain not found. Running CMake without explicit toolchain file."
fi

print_header "Configure (Release)"
cmake "${CMAKE_ARGS[@]}" -DCMAKE_BUILD_TYPE=Release
print_ok "CMake configured"

print_header "Build (Release)"
cmake --build "${BUILD_DIR}" --config Release --parallel
print_ok "Release build completed"

SIMULATOR="${BUILD_DIR}/bin/simulator"
case "$(uname -s)" in
    CYGWIN*|MINGW*|MSYS*) SIMULATOR="${BUILD_DIR}/bin/simulator.exe" ;;
esac

if [ ! -f "${SIMULATOR}" ]; then
    echo "✗ simulator binary not found: ${SIMULATOR}"
    exit 1
fi
print_ok "Simulator found: ${SIMULATOR}"

print_header "Configure tests (Debug)"
cmake "${CMAKE_ARGS[@]}" -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON
print_ok "Debug configured"

print_header "Build tests (Debug)"
cmake --build "${BUILD_DIR}" --config Debug --parallel
print_ok "Debug build completed"

print_header "Run unit tests"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
print_ok "Tests completed"

print_header "Optional: clang-tidy"
if command -v clang-tidy >/dev/null 2>&1; then
    print_ok "clang-tidy detected (manual invocation available)"
else
    print_warn "clang-tidy not installed (skipped)"
fi

print_header "Local CI smoke pipeline completed"
