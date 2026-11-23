#!/usr/bin/env bash
set -euo pipefail

# Caches accelerate setup but are never required for correctness.
baseline=$(python3 -c "import json; print(json.load(open('vcpkg.json'))['builtin-baseline'])")
[[ "$baseline" =~ ^[0-9a-f]{40}$ ]] || { echo 'Invalid vcpkg baseline'; exit 1; }
if [[ ! -d vcpkg ]]; then
  git clone https://github.com/microsoft/vcpkg.git vcpkg
fi
git -C vcpkg checkout --detach "$baseline"
if [[ ! -x vcpkg/vcpkg ]]; then
  ./vcpkg/bootstrap-vcpkg.sh -disableMetrics
fi
