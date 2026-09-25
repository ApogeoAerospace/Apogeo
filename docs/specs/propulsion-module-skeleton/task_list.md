# Propulsion module skeleton — work plan

Status: Current — completed. Plan recorded before source implementation.

## Ordered work packages

- [x] 1. Inspect active modules, historical propulsion, host lifecycle and build layout.
- [x] 2. Establish functional scope, CSV contract, interfaces, integration decisions,
  research references and acceptance-to-test mapping.
- [x] 3. Implement typed engine data and strict CSV reader; preserve data on reload failure.
- [x] 4. Implement interpolation and explicit pending-physics interfaces.
- [x] 5. Add C ABI adapter with configuration-time loading and neutral verified ticks.
- [x] 6. Register root CMake targets, mock deployment and disabled example configuration.
- [x] 7. Add initialization/parser/interpolation/lifecycle tests under the module.
- [x] 8. Build, run targeted and existing tests, inspect diff, record results and limits.

## Verification results

- Direct C++17 compilation with `-Wall -Wextra -Wpedantic`: passed without warnings.
- Direct execution of `propulsion_tests`: 7/7 passed, including 23 malformed CSV variants.
- Root CMake configuration: passed in `build/propulsion-root-check` using local Boost
  1.88.0 and Eigen 3.4.0 sources, installed FlatBuffers/JSON and existing GTest.
- Original vcpkg configuration did not complete promptly; dependency sources were
  downloaded into ignored `build/propulsion-deps` for an independent root build.
- Full root compilation: passed, including `simulator`, `structures`, `propulsion`,
  `core_tests` and `propulsion_tests`.
- CTest: **127/127 passed**, 0 failures (120 existing + 7 propulsion), 6.76 seconds.
- Real host smoke: enabled propulsion in a temporary copy of the default config;
  simulator loaded/configured both structures and propulsion, completed 2 ticks,
  and exited successfully. The checked-in default remains disabled.
- `git diff --check`: passed.

## Reproduction of this environment's root verification

The original `build` cache uses vcpkg. An independent build avoided its stalled
installation without changing `vcpkg.json`. Official Boost 1.88.0 and Eigen 3.4.0
archives were extracted under ignored `build/propulsion-deps`. Installed Homebrew
FlatBuffers/JSON and the already available GTest package were used.

```bash
cmake -S . -B build/propulsion-root-check -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_POLICY_DEFAULT_CMP0167=OLD \
  -DBoost_NO_BOOST_CMAKE=ON \
  -DBOOST_ROOT="$PWD/build/propulsion-deps/boost_1_88_0" \
  -DFETCHCONTENT_SOURCE_DIR_EIGEN="$PWD/build/propulsion-deps/eigen-3.4.0" \
  -DGTest_DIR="$PWD/build/vcpkg_installed/arm64-osx/share/gtest"
cmake --build build/propulsion-root-check -j 4
ctest --test-dir build/propulsion-root-check --output-on-failure
```

The temporary smoke config was generated from `data/defaults/default_config.json`
with only the propulsion plugin enabled, then run from `build/propulsion-root-check/bin`:

```bash
./simulator --config ../propulsion-smoke.json --ticks 2
```

Validation platform: macOS arm64, AppleClang 17, C++17, Debug root build. Other
platforms have not been executed. Existing CMake policy/deprecation warnings and a
core-test duplicate GTest library linker warning remain outside this feature.
No thermodynamic accuracy or Merlin validation is claimed by these results.

## Follow-up scope

- Implement the engine-cycle calculations currently returning `std::nullopt`.
- Establish real dataset provenance and model-specific operating domains.
- Decide additional axes and interpolation strategies when throttle/mixture vary.
- Define how fuel consumption reaches the host without parallel state mutation.
- Consider optional host logging and database ingestion when those are required.
