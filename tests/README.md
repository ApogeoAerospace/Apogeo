# 🧪 Tests Unitarios de MoLab

Este directorio contiene los tests unitarios del proyecto MoLab Aerospace Simulator, implementados con **Google Test**.

## ⚠️ Consideraciones Importantes

### Singletons en MoLab
Varios componentes del proyecto usan el patrón Singleton:
- `ConfigManager`
- `TimeManager`
- `Logger`
- `OutputManager`

**❌ Incorrecto:**
```cpp
ConfigManager config_manager;  // Error de compilación
```

**✅ Correcto:**
```cpp
auto& config_manager = ConfigManager::getInstance();
```

### Namespace MoLab
Todas las clases están en `namespace MoLab`. Usa:
```cpp
using namespace MoLab;
```

### Manejo de Errores
- `ConfigManager::loadConfig()` **NO lanza excepciones**, retorna `bool`
- En caso de error, usa valores por defecto y retorna `false`

## 📋 Estructura de Tests

```
tests/
├── test_config_manager.cpp      # Tests del gestor de configuración (10 tests)
├── test_physics_integrator.cpp  # Tests del integrador de física (10 tests)
└── test_time_manager.cpp        # Tests del gestor de tiempo (13 tests)
```

**Total: 33 tests unitarios**

## 🚀 Ejecutar Tests Localmente

### Compilar con tests habilitados:

```bash
# Configurar CMake con tests
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug

# Compilar
cmake --build build --parallel

# Ejecutar todos los tests
cd build
ctest --output-on-failure --verbose
```

### Ejecutar tests específicos:

```bash
# Solo tests de ConfigManager
cd build
./bin/core_tests --gtest_filter=ConfigManagerTest.*

# Solo tests de PhysicsIntegrator
./bin/core_tests --gtest_filter=PhysicsIntegratorTest.*

# Solo tests de TimeManager
./bin/core_tests --gtest_filter=TimeManagerTest.*
```

## 📊 Cobertura de Código

### Generar reporte de cobertura:

```bash
# Configurar con cobertura habilitada
cmake -B build -S . \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DENABLE_TESTING=ON \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_BUILD_TYPE=Debug

# Compilar y ejecutar tests
cmake --build build --parallel
cd build
ctest

# Generar reporte con lcov
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/vcpkg_installed/*' '*/tests/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_html

# Abrir reporte en navegador
open coverage_html/index.html  # macOS
xdg-open coverage_html/index.html  # Linux
```

## 🔍 Tests Implementados

### ConfigManagerTest (10 tests)
- ✅ Carga de configuración válida
- ✅ Obtención de time_step
- ✅ Obtención de duración
- ✅ Obtención de max_iterations
- ✅ Manejo de archivos inexistentes (usa defaults)
- ✅ Verificación de gravedad habilitada
- ✅ Tipo de integrador
- ✅ Validación de configuración
- ✅ Obtención de archivo de estado inicial
- ✅ Obtención de directorio de salida

### PhysicsIntegratorTest (10 tests)
- ✅ Creación de integrador
- ✅ Cambio de tipo de integrador (enum)
- ✅ Cambio de tipo de integrador (string)
- ✅ Obtención de nombre de integrador
- ✅ Integración de Euler con fuerza constante
- ✅ Integración con fuerza cero
- ✅ Operaciones de Vector3 (suma, resta, multiplicación)
- ✅ Magnitud de Vector3
- ✅ Normalización de Vector3

### TimeManagerTest (13 tests)
- ✅ Obtención de instancia singleton
- ✅ Tiempo de simulación inicial
- ✅ Actualización de tiempo de simulación
- ✅ Múltiples actualizaciones de tiempo
- ✅ Reset de tiempo
- ✅ Establecer tiempo de simulación
- ✅ Consistencia de time steps
- ✅ Inicialización con tiempo personalizado
- ✅ Obtención de UTC actual
- ✅ Conversión de tiempo de simulación a UTC
- ✅ Conversión de UTC a tiempo de simulación
- ✅ Obtención de string UTC

## 📝 Escribir Nuevos Tests

### Template básico:

```cpp
#include <gtest/gtest.h>
#include "TuClase.h"

class TuClaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Inicialización antes de cada test
    }

    void TearDown() override {
        // Limpieza después de cada test
    }

    // Variables compartidas entre tests
};

TEST_F(TuClaseTest, NombreDelTest) {
    // Arrange (preparar)
    TuClase objeto;
    
    // Act (actuar)
    int resultado = objeto.metodo();
    
    // Assert (verificar)
    EXPECT_EQ(resultado, valor_esperado);
}
```

### Macros útiles de Google Test:

```cpp
// Comparaciones
EXPECT_EQ(a, b);      // a == b
EXPECT_NE(a, b);      // a != b
EXPECT_LT(a, b);      // a < b
EXPECT_LE(a, b);      // a <= b
EXPECT_GT(a, b);      // a > b
EXPECT_GE(a, b);      // a >= b

// Comparaciones de punto flotante
EXPECT_DOUBLE_EQ(a, b);        // Igualdad exacta
EXPECT_NEAR(a, b, epsilon);    // Igualdad con tolerancia

// Booleanos
EXPECT_TRUE(condicion);
EXPECT_FALSE(condicion);

// Excepciones
EXPECT_THROW(statement, exception_type);
EXPECT_NO_THROW(statement);
```

## 🎯 Cobertura Objetivo

| Componente | Cobertura Objetivo | Estado Actual |
|------------|-------------------|---------------|
| ConfigManager | 80% | 🟡 En progreso |
| PhysicsIntegrator | 80% | 🟡 En progreso |
| TimeManager | 80% | 🟡 En progreso |
| SimulationEngine | 70% | 🔴 Pendiente |
| PluginManager | 70% | 🔴 Pendiente |
| OutputManager | 70% | 🔴 Pendiente |

## 🚨 Troubleshooting

### Error: "GTest not found"
```bash
# Reinstalar dependencias con vcpkg
vcpkg install gtest
```

### Error: "undefined reference to `testing::*`"
Verificar en `CMakeLists.txt`:
```cmake
target_link_libraries(core_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
)
```

### Tests no se ejecutan
```bash
# Verificar que CTest detecta los tests
cd build
ctest -N  # Lista tests sin ejecutarlos
```

## 📚 Recursos

- [Google Test Documentation](https://google.github.io/googletest/)
- [Google Test Primer](https://google.github.io/googletest/primer.html)
- [Advanced Google Test](https://google.github.io/googletest/advanced.html)

## 🔄 Integración Continua

Los tests se ejecutan automáticamente en GitHub Actions en cada push/PR:
- **Job `unit-tests`**: Ejecuta todos los tests unitarios
- **Job `code-coverage`**: Genera reporte de cobertura
- Los reportes están disponibles en la pestaña "Actions" → "Artifacts"
