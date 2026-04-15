# 📊 Visual CI Pipeline Summary

```
┌─────────────────────────────────────────────────────────────────┐
│                    MoLab CI Pipeline                            │
│                    (6 Jobs in Parallel)                         │
└─────────────────────────────────────────────────────────────────┘

┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│  build-and-test  │  │  code-quality    │  │  unit-tests      │
│                  │  │                  │  │                  │
│ • Ubuntu         │  │ • Formatting     │  │ • Google Test    │
│ • macOS          │  │ • JSON           │  │ • CTest          │
│ • C++17          │  │ • FlatBuffers    │  │ • 3 test suites  │
│ • vcpkg          │  │                  │  │                  │
│ • 6 plugins      │  │                  │  │                  │
└────────┬─────────┘  └──────────────────┘  └──────────────────┘
         │
         ├─────────────────────────────────────────────────────┐
         │                                                     │
         ▼                                                     ▼
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│ integration-test │  │  code-coverage   │  │ static-analysis  │
│                  │  │                  │  │                  │
│ • Simulation     │  │ • lcov           │  │ • clang-tidy     │
│ • basic_config   │  │ • genhtml        │  │ • 7 categorias   │
│ • 10 ticks       │  │ • HTML report    │  │ • compile_cmds   │
│ • Output verify  │  │ • 30 days        │  │ • Error detect   │
└──────────────────┘  └──────────────────┘  └──────────────────┘
```

## 🎯 Execution Flow

### Phase 1: Build and Basic Validation (Parallel)
```
[build-and-test] ──┐
                   ├──> Artifacts: binaries
[code-quality]  ───┤
                   └──> Validation: formatting, JSON
[unit-tests]    ───┐
                   └──> Tests: Google Test
```

### Phase 2: Advanced Tests (Parallel, some depend on Phase 1)
```
[integration-test] ──> Uses build-and-test artifacts
[code-coverage]    ──> Independent, generates HTML report
[static-analysis]  ──> Independent, generates TXT report
```

## 📦 Generated Artifacts

| Artifact | Job | Retention | Content |
|-----------|-----|-----------|-----------|
| `molab-build-ubuntu-latest` | build-and-test | 7 days | Linux binaries |
| `molab-build-macos-latest` | build-and-test | 7 days | macOS binaries |
| `test-results` | unit-tests | 7 days | XML test results |
| `coverage-report` | code-coverage | 30 days | HTML coverage report |
| `static-analysis-report` | static-analysis | 30 days | clang-tidy TXT report |

## ⏱️ Estimated Execution Time

| Job | Approximate Duration |
|-----|---------------------|
| build-and-test (Ubuntu) | ~8-12 min |
| build-and-test (macOS) | ~10-15 min |
| code-quality | ~2-3 min |
| integration-test | ~3-5 min |
| unit-tests | ~6-8 min |
| code-coverage | ~8-10 min |
| static-analysis | ~5-7 min |

**Total (parallel)**: ~15-20 minutes

## 🔍 What Is Validated in Each Job

### ✅ build-and-test
- [x] Successful build on Ubuntu and macOS
- [x] `simulator` binary generation
- [x] Build of 6 dynamic plugins
- [x] FlatBuffers schema generation
- [x] Python script syntax validation

### ✅ code-quality
- [x] No trailing whitespace at end of lines
- [x] All JSON files well-formed
- [x] FlatBuffers schemas present

### ✅ integration-test
- [x] Executable simulation with real config
- [x] Output file generation
- [x] Log generation

### ✅ unit-tests
- [x] ConfigManager: 5 tests
- [x] PhysicsIntegrator: 5 tests
- [x] TimeManager: 6 tests
- [x] Total: 16 unit tests

### ✅ code-coverage
- [x] Line coverage
- [x] Function coverage
- [x] Branch coverage
- [x] Navigable HTML report

### ✅ static-analysis
- [x] Potential bug detection
- [x] C++ Core Guidelines violations
- [x] Code smells
- [x] Modernization suggestions
- [x] Performance issues

## 🚦 Success Criteria

The pipeline **PASSES** if:
- ✅ All builds compile without errors
- ✅ All unit tests pass
- ✅ No critical errors in static analysis
- ✅ JSON files are valid
- ✅ Integration simulation runs

The pipeline **FAILS** if:
- ❌ Build fails on any platform
- ❌ Any unit test fails
- ❌ clang-tidy reports critical errors
- ❌ Malformed JSON files
- ❌ Trailing whitespace is found

## 📈 Quality Metrics

### Code Coverage (Target)
- **Lines**: ≥ 70%
- **Functions**: ≥ 75%
- **Branches**: ≥ 60%

### Complexity (Limits)
- **Cyclomatic complexity**: ≤ 15 per function
- **Cognitive complexity**: ≤ 50 per function
- **Lines per function**: ≤ 100

### Static Analysis
- **Critical errors**: 0
- **Warnings**: Minimize
- **Code smells**: Review and fix

## 🔧 Quick Commands

### View CI status:
```bash
# On GitHub
https://github.com/tu-usuario/MoLab/actions
```

### Run locally (CI equivalent):
```bash
# Full build
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel

# Tests
cd build && ctest --output-on-failure

# Coverage
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/vcpkg_installed/*' '*/tests/*' -o coverage_filtered.info
genhtml coverage_filtered.info -o coverage_html

# Static analysis
find core src -name '*.cpp' | xargs clang-tidy -p build
```

## 📊 CI Dashboard

After each execution, review:

1. **Actions Tab** → Overall status
2. **Artifacts** → Download reports
3. **Coverage Report** → Review coverage
4. **Static Analysis** → Review warnings

## 🎓 Resources

- [GitHub Actions Docs](https://docs.github.com/en/actions)
- [Google Test Docs](https://google.github.io/googletest/)
- [lcov Manual](http://ltp.sourceforge.net/coverage/lcov.php)
- [Clang-Tidy Checks](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
