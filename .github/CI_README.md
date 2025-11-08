# 🚀 Documentación del CI Pipeline de MoLab

## Descripción General

Este pipeline de CI (Continuous Integration) valida automáticamente el código del proyecto **MoLab Aerospace Simulator** en cada push y pull request a las ramas `main` y `develop`.

## 📋 Estructura del Pipeline

El pipeline está dividido en **3 jobs principales**:

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

## 🔧 Dependencias del Proyecto

### Dependencias C++ (gestionadas por vcpkg):
- **flatbuffers** → Serialización eficiente de datos
- **nlohmann-json** → Manejo de archivos JSON

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

## 📈 Mejoras Futuras

### Sugerencias para ampliar el CI:

1. **Tests Unitarios con Google Test**:
   ```cmake
   enable_testing()
   add_subdirectory(tests)
   ```

2. **Cobertura de Código** (gcov/lcov):
   ```bash
   cmake -DCMAKE_BUILD_TYPE=Coverage
   ```

3. **Análisis Estático**:
   - Clang-Tidy
   - Cppcheck
   - ASAN/UBSAN

4. **Benchmarks de Performance**:
   - Medir tiempo de simulación
   - Comparar con baseline

5. **Tests de Escenarios Realistas**:
   - Ejecutar `test_realistic_scenarios.sh` en CI
   - Validar métricas físicas (altitud, velocidad)

6. **Docker Containers**:
   - Ejecutar en contenedores para consistencia
   - Probar en diferentes distribuciones Linux

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
