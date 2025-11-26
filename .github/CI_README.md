# 🚀 Documentación del CI Pipeline de MoLab

## Descripción General

Este pipeline de CI (Continuous Integration) valida automáticamente el código del proyecto **MoLab Aerospace Simulator** en cada push y pull request a las ramas `main` y `develop`.

## 📋 Estructura del Pipeline

El pipeline está dividido en **6 jobs principales**:

### 1️⃣ **build-and-test** - Compilación Multi-plataforma
**Objetivo**: Compilar el núcleo C++, plugins y validar en múltiples plataformas.

#### Matriz de ejecución:
- **Ubuntu Latest** (Linux x64)
- **macOS Latest** (macOS x64)

#### Pasos:
1. **Checkout del código** con submódulos recursivos
2. **Configurar Python 3.11** para herramientas de análisis
3. **Instalar dependencias del sistema**:
   - Ubuntu: `build-essential`, `cmake`, `ninja-build`, `python3-tk`, `pkg-config`
   - macOS: `cmake`, `ninja`, `pkg-config` (vía Homebrew)
4. **Configurar vcpkg** para gestionar dependencias C++ (flatbuffers, nlohmann-json)
5. **Configurar CMake** con vcpkg toolchain y Ninja generator
6. **Compilar el proyecto** en modo Release con paralelización
7. **Verificar binarios** generados (simulator + plugins)
8. **Validar sintaxis de scripts Python** en `tools/`
9. **Subir artefactos** de build para análisis posterior

#### Artefactos generados:
- `molab-build-ubuntu-latest` → Binarios Linux
- `molab-build-macos-latest` → Binarios macOS
- Retención: 7 días

---

### 2️⃣ **code-quality** - Validación de Calidad
**Objetivo**: Verificar calidad del código y archivos de configuración.

#### Validaciones:
1. **Formato de archivos C++**:
   - Detecta espacios en blanco al final de líneas
   - Archivos: `*.cpp`, `*.h`, `*.hpp`
   
2. **Validar archivos JSON**:
   - Verifica sintaxis de todos los archivos en `data/`
   - Detecta JSON malformados antes de runtime
   
3. **Esquemas FlatBuffers**:
   - Lista esquemas `.fbs` disponibles en `src/schemas/`

---

### 3️⃣ **integration-test** - Pruebas de Integración
**Objetivo**: Ejecutar el simulador con configuración real.

#### Pasos:
1. **Descargar artefactos** de build de Ubuntu
2. **Configurar permisos** de ejecución para binarios y plugins
3. **Ejecutar simulación de prueba**:
   - Config: `data/config/basic_config.json`
   - Ticks: 10 (simulación corta)
4. **Verificar salida**:
   - Archivos de output generados
   - Logs de simulación

**Nota**: Este job depende de `build-and-test` (solo corre si la compilación es exitosa).

---

### 4️⃣ **unit-tests** - Tests Unitarios con Google Test
**Objetivo**: Ejecutar tests unitarios del código C++ con Google Test.

#### Pasos:
1. **Instalar dependencias** incluyendo `lcov` para cobertura
2. **Configurar vcpkg** para obtener Google Test
3. **Compilar en modo Debug** con `-DENABLE_TESTING=ON`
4. **Ejecutar tests** con CTest
5. **Subir resultados** como artefactos

#### Tests ejecutados:
- `ConfigManagerTest` → Tests del gestor de configuración
- `PhysicsIntegratorTest` → Tests del integrador de física
- `TimeManagerTest` → Tests del gestor de tiempo

#### Salida:
- Logs detallados de cada test
- Artefacto `test-results` con resultados XML

---

### 5️⃣ **code-coverage** - Cobertura de Código
**Objetivo**: Medir la cobertura de código de los tests unitarios.

#### Herramientas:
- **lcov** → Captura datos de cobertura
- **gcovr** → Generación de reportes alternativos
- **genhtml** → Generación de reporte HTML interactivo

#### Pasos:
1. **Compilar con flags de cobertura** (`--coverage`, `-fprofile-arcs`, `-ftest-coverage`)
2. **Ejecutar todos los tests** para generar datos `.gcda`
3. **Capturar cobertura** con `lcov`
4. **Filtrar archivos** (excluye `/usr/*`, `vcpkg_installed`, `tests`)
5. **Generar reporte HTML** navegable
6. **Subir artefacto** con reporte completo (30 días)

#### Métricas reportadas:
- **Cobertura de líneas** (line coverage)
- **Cobertura de funciones** (function coverage)
- **Cobertura de branches** (branch coverage)

#### Visualización:
- Descarga el artefacto `coverage-report`
- Abre `index.html` en navegador
- Navega por archivos con código resaltado (cubierto/no cubierto)

---

### 6️⃣ **static-analysis** - Análisis Estático con Clang-Tidy
**Objetivo**: Detectar bugs potenciales, code smells y violaciones de estilo.

#### Herramienta:
- **clang-tidy** → Analizador estático de C++

#### Checks habilitados:
- `bugprone-*` → Detecta bugs comunes
- `cert-*` → Estándares de seguridad CERT
- `clang-analyzer-*` → Análisis profundo del compilador
- `cppcoreguidelines-*` → C++ Core Guidelines
- `modernize-*` → Sugerencias de C++ moderno
- `performance-*` → Optimizaciones de rendimiento
- `readability-*` → Mejoras de legibilidad

#### Pasos:
1. **Instalar clang-tidy** y herramientas
2. **Generar compile_commands.json** con CMake
3. **Analizar archivos** en `core/` y `src/`
4. **Verificar errores críticos** (falla el build si hay errores)
5. **Subir reporte** completo como artefacto

#### Configuración:
El archivo `.clang-tidy` define:
- Checks activos/deshabilitados
- Convenciones de nombres (CamelCase, snake_case)
- Umbrales de complejidad

---

## 🔧 Dependencias del Proyecto

### Dependencias C++ (gestionadas por vcpkg):
- **flatbuffers** → Serialización eficiente de datos
- **nlohmann-json** → Manejo de archivos JSON
- **gtest** → Framework de tests unitarios

### Dependencias del Sistema:
- **CMake** ≥ 3.20
- **Compilador C++17** (GCC/Clang/MSVC)
- **Python** ≥ 3.x
- **Ninja Build** (opcional, mejora velocidad)

---

## 📊 Componentes Compilados

### Ejecutable Principal:
- `simulator` → Motor de simulación principal

### Biblioteca Core:
- `libcore.a` → Núcleo con:
  - `SimulationEngine`
  - `PluginManager`
  - `ConfigManager`
  - `PhysicsIntegrator`
  - `OutputManager`
  - `TimeManager`

### Plugins Dinámicos:
1. `example_plugin.so/.dylib`
2. `aerodynamics.so/.dylib`
3. `propulsion.so/.dylib`
4. `environment.so/.dylib`
5. `structures.so/.dylib`
6. `programming.so/.dylib`

---

## ⚡ Optimizaciones del Pipeline

### Concurrencia:
```yaml
concurrency:
  group: ci-${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```
- Cancela builds obsoletos en pushes sucesivos al mismo PR
- Ahorra minutos de CI

### Caché de vcpkg:
- El action `lukka/run-vcpkg@v11` cachea automáticamente dependencias
- Reducción de ~5 minutos en builds subsecuentes

### Compilación Paralela:
```bash
cmake --build build --config Release --parallel
```
- Utiliza todos los cores disponibles

---

## 🚨 Fallos Comunes y Soluciones

### ❌ Error: "flatc not found"
**Causa**: vcpkg no instaló FlatBuffers correctamente.

**Solución**:
```bash
# Limpiar vcpkg y reinstalar
rm -rf vcpkg_installed/
vcpkg install flatbuffers nlohmann-json
```

### ❌ Error: "undefined reference to flatbuffers::*"
**Causa**: Linking incorrecto con flatbuffers.

**Solución**: Verificar en `CMakeLists.txt`:
```cmake
target_link_libraries(core PUBLIC flatbuffers::flatbuffers)
```

### ❌ Error: "basic_config.json not found"
**Causa**: El test de integración no encuentra archivos de data.

**Solución**: Verificar que `data/config/basic_config.json` existe en el repo.

---

## 🧪 Ejecutar Validaciones Localmente

### Compilación completa:
```bash
# Instalar dependencias (primera vez)
./install_dependencies.sh

# Compilar con vcpkg
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --parallel
```

### Ejecutar tests unitarios:
```bash
# Compilar con tests
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel

# Ejecutar tests
cd build
ctest --output-on-failure --verbose
```

### Generar cobertura de código:
```bash
# Compilar con cobertura
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_BUILD_TYPE=Debug

cmake --build build --parallel
cd build
ctest

# Generar reporte
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/vcpkg_installed/*' '*/tests/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_html

# Abrir reporte
open coverage_html/index.html  # macOS
xdg-open coverage_html/index.html  # Linux
```

### Ejecutar análisis estático:
```bash
# Generar compile_commands.json
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Ejecutar clang-tidy
find core src -name '*.cpp' | xargs clang-tidy -p build --config-file=.clang-tidy
```

### Validar JSON:
```bash
find data -name '*.json' -type f -exec python3 -m json.tool {} \;
```

### Validar sintaxis Python:
```bash
python3 -m py_compile tools/*.py
```

### Ejecutar test de integración:
```bash
cd build/bin
./simulator --config ../../data/config/basic_config.json --ticks 10
```

---

## 📈 Mejoras Futuras Sugeridas

### Ampliaciones posibles del CI:

1. ✅ **Tests Unitarios con Google Test** - ✅ IMPLEMENTADO
2. ✅ **Cobertura de Código** (gcov/lcov) - ✅ IMPLEMENTADO
3. ✅ **Análisis Estático** (Clang-Tidy) - ✅ IMPLEMENTADO

4. **Sanitizers** (próxima fase):
   - AddressSanitizer (ASAN) → Detecta memory leaks
   - UndefinedBehaviorSanitizer (UBSAN) → Detecta UB
   - ThreadSanitizer (TSAN) → Detecta race conditions

5. **Benchmarks de Performance**:
   - Google Benchmark para medir rendimiento
   - Comparar con baseline en cada PR
   - Detectar regresiones de performance

6. **Tests de Escenarios Realistas**:
   - Ejecutar `test_realistic_scenarios.sh` en CI
   - Validar métricas físicas (altitud, velocidad)
   - Comparar con valores esperados

7. **Docker Containers**:
   - Ejecutar en contenedores para consistencia
   - Probar en diferentes distribuciones Linux
   - Imágenes pre-construidas con dependencias

8. **Cppcheck**:
   - Análisis estático adicional
   - Complementa clang-tidy

9. **Integración con Codecov/Coveralls**:
   - Visualización de cobertura en PRs
   - Tracking histórico de cobertura

---

## 📞 Soporte

Si encuentras problemas con el CI:
1. Revisa los **logs de GitHub Actions** en la pestaña "Actions"
2. Verifica que las dependencias locales coincidan con las del CI
3. Ejecuta las validaciones localmente antes de push
4. Consulta `CONTRIBUTING.md` para guías de desarrollo

---

## 🔒 Permisos y Seguridad

El workflow tiene permisos mínimos:
```yaml
permissions:
  contents: read  # Solo lectura del código
```

No requiere:
- Escritura de código
- Acceso a secrets
- Permisos de deployment

---

**Última actualización**: 2025-11-07  
**Versión del workflow**: 1.0.0
