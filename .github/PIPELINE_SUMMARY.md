# 📊 Resumen Visual del CI Pipeline

```
┌─────────────────────────────────────────────────────────────────┐
│                    MoLab CI Pipeline                            │
│                  (6 Jobs en Paralelo)                           │
└─────────────────────────────────────────────────────────────────┘

┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│  build-and-test  │  │  code-quality    │  │  unit-tests      │
│                  │  │                  │  │                  │
│ • Ubuntu         │  │ • Formato        │  │ • Google Test    │
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
│ • Simulación     │  │ • lcov           │  │ • clang-tidy     │
│ • basic_config   │  │ • genhtml        │  │ • 7 categorías   │
│ • 10 ticks       │  │ • HTML report    │  │ • compile_cmds   │
│ • Output verify  │  │ • 30 días        │  │ • Error detect   │
└──────────────────┘  └──────────────────┘  └──────────────────┘
```

## 🎯 Flujo de Ejecución

### Fase 1: Compilación y Validación Básica (Paralelo)
```
[build-and-test] ──┐
                   ├──> Artefactos: binarios
[code-quality]  ───┤
                   └──> Validación: formato, JSON
[unit-tests]    ───┐
                   └──> Tests: Google Test
```

### Fase 2: Tests Avanzados (Paralelo, algunos dependen de Fase 1)
```
[integration-test] ──> Usa artefactos de build-and-test
[code-coverage]    ──> Independiente, genera reporte HTML
[static-analysis]  ──> Independiente, genera reporte TXT
```

## 📦 Artefactos Generados

| Artefacto | Job | Retención | Contenido |
|-----------|-----|-----------|-----------|
| `molab-build-ubuntu-latest` | build-and-test | 7 días | Binarios Linux |
| `molab-build-macos-latest` | build-and-test | 7 días | Binarios macOS |
| `test-results` | unit-tests | 7 días | Resultados XML de tests |
| `coverage-report` | code-coverage | 30 días | Reporte HTML de cobertura |
| `static-analysis-report` | static-analysis | 30 días | Reporte TXT de clang-tidy |

## ⏱️ Tiempo Estimado de Ejecución

| Job | Duración Aproximada |
|-----|---------------------|
| build-and-test (Ubuntu) | ~8-12 min |
| build-and-test (macOS) | ~10-15 min |
| code-quality | ~2-3 min |
| integration-test | ~3-5 min |
| unit-tests | ~6-8 min |
| code-coverage | ~8-10 min |
| static-analysis | ~5-7 min |

**Total (paralelo)**: ~15-20 minutos

## 🔍 Qué se Valida en Cada Job

### ✅ build-and-test
- [x] Compilación exitosa en Ubuntu y macOS
- [x] Generación de binario `simulator`
- [x] Compilación de 6 plugins dinámicos
- [x] Generación de FlatBuffers schemas
- [x] Sintaxis de scripts Python

### ✅ code-quality
- [x] Sin espacios en blanco al final de líneas
- [x] Todos los JSON bien formados
- [x] Esquemas FlatBuffers presentes

### ✅ integration-test
- [x] Simulación ejecutable con config real
- [x] Generación de archivos de output
- [x] Generación de logs

### ✅ unit-tests
- [x] ConfigManager: 5 tests
- [x] PhysicsIntegrator: 5 tests
- [x] TimeManager: 6 tests
- [x] Total: 16 tests unitarios

### ✅ code-coverage
- [x] Cobertura de líneas
- [x] Cobertura de funciones
- [x] Cobertura de branches
- [x] Reporte HTML navegable

### ✅ static-analysis
- [x] Detección de bugs potenciales
- [x] Violaciones de C++ Core Guidelines
- [x] Code smells
- [x] Sugerencias de modernización
- [x] Problemas de rendimiento

## 🚦 Criterios de Éxito

El pipeline **PASA** si:
- ✅ Todos los builds compilan sin errores
- ✅ Todos los tests unitarios pasan
- ✅ No hay errores críticos en análisis estático
- ✅ Archivos JSON son válidos
- ✅ Simulación de integración se ejecuta

El pipeline **FALLA** si:
- ❌ Falla compilación en cualquier plataforma
- ❌ Algún test unitario falla
- ❌ clang-tidy reporta errores críticos
- ❌ JSON malformados
- ❌ Espacios en blanco al final de líneas

## 📈 Métricas de Calidad

### Cobertura de Código (Objetivo)
- **Líneas**: ≥ 70%
- **Funciones**: ≥ 75%
- **Branches**: ≥ 60%

### Complejidad (Límites)
- **Complejidad ciclomática**: ≤ 15 por función
- **Complejidad cognitiva**: ≤ 50 por función
- **Líneas por función**: ≤ 100

### Análisis Estático
- **Errores críticos**: 0
- **Advertencias**: Minimizar
- **Code smells**: Revisar y corregir

## 🔧 Comandos Rápidos

### Ver estado del CI:
```bash
# En GitHub
https://github.com/tu-usuario/MoLab/actions
```

### Ejecutar localmente (equivalente al CI):
```bash
# Build completo
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel

# Tests
cd build && ctest --output-on-failure

# Cobertura
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/vcpkg_installed/*' '*/tests/*' -o coverage_filtered.info
genhtml coverage_filtered.info -o coverage_html

# Análisis estático
find core src -name '*.cpp' | xargs clang-tidy -p build
```

## 📊 Dashboard de CI

Después de cada ejecución, revisa:

1. **Actions Tab** → Estado general
2. **Artifacts** → Descarga reportes
3. **Coverage Report** → Visualiza cobertura
4. **Static Analysis** → Revisa advertencias

## 🎓 Recursos

- [GitHub Actions Docs](https://docs.github.com/en/actions)
- [Google Test Docs](https://google.github.io/googletest/)
- [lcov Manual](http://ltp.sourceforge.net/coverage/lcov.php)
- [Clang-Tidy Checks](https://clang.llvm.org/extra/clang-tidy/checks/list.html)
