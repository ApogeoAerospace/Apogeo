# ✅ Mejores Prácticas para Tests en MoLab

## 🎯 Resumen de Correcciones Implementadas

Los tests iniciales tenían **problemas críticos** que impedían su compilación y ejecución. Aquí está lo que se corrigió:

---

## 🔴 Problemas Encontrados y Soluciones

### 1. **Uso Incorrecto de Singletons**

#### ❌ Problema Original:
```cpp
ConfigManager config_manager;  // Error: constructor privado
TimeManager* time_manager = new TimeManager();  // Error: constructor privado
```

#### ✅ Solución Correcta:
```cpp
auto& config_manager = ConfigManager::getInstance();
auto& time_manager = TimeManager::getInstance();
```

**Razón:** `ConfigManager` y `TimeManager` usan el patrón Singleton con constructores privados.

---

### 2. **Namespace Faltante**

#### ❌ Problema Original:
```cpp
#include "ConfigManager.h"

ConfigManager config_manager;  // Error: ConfigManager no está en scope
```

#### ✅ Solución Correcta:
```cpp
#include "ConfigManager.h"

using namespace MoLab;  // Todas las clases están en este namespace

auto& config_manager = ConfigManager::getInstance();
```

---

### 3. **Expectativas Incorrectas de Excepciones**

#### ❌ Problema Original:
```cpp
EXPECT_THROW(config_manager.loadConfig("non_existent.json"), std::exception);
```

**Problema:** `loadConfig()` NO lanza excepciones, retorna `false` y usa defaults.

#### ✅ Solución Correcta:
```cpp
// loadConfig retorna false pero NO lanza excepción
EXPECT_FALSE(config_manager.loadConfig("non_existent.json"));

// Verifica que usa valores por defecto
const auto& sim_config = config_manager.getSimulationConfig();
EXPECT_GT(sim_config.time_step, 0.0);
```

---

### 4. **Métodos Inexistentes**

#### ❌ Problema Original:
```cpp
// PhysicsIntegrator
double time_step = config_manager.getTimeStep();  // Método no existe
bool gravity = config_manager.isGravityEnabled();  // Método no existe

// TimeManager
time_manager->advance(dt);  // Método no existe
double delta = time_manager->getDeltaTime();  // Método no existe
```

#### ✅ Solución Correcta:
```cpp
// ConfigManager - usar getters de structs
const auto& sim_config = config_manager.getSimulationConfig();
double time_step = sim_config.time_step;

const auto& physics_config = config_manager.getPhysicsConfig();
bool gravity = physics_config.enable_gravity;

// TimeManager - usar métodos reales
time_manager.updateSimulationTime(dt);  // No "advance"
double sim_time = time_manager.getSimulationTime();  // No "getDeltaTime"
```

---

### 5. **Estructura de Tests Incorrecta**

#### ❌ Problema Original:
```cpp
class PhysicsIntegratorTest : public ::testing::Test {
protected:
    PhysicsIntegrator* integrator;  // Puntero sin inicializar correctamente
};
```

#### ✅ Solución Correcta:
```cpp
class PhysicsIntegratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        integrator = new PhysicsIntegrator(PhysicsIntegrator::IntegratorType::EULER);
    }
    
    void TearDown() override {
        delete integrator;
    }
    
    PhysicsIntegrator* integrator;
};
```

---

### 6. **Tests de Física Simplistas**

#### ❌ Problema Original:
```cpp
// Tests que no usan la API real de PhysicsIntegrator
double new_velocity = velocity + acceleration * dt;
EXPECT_NEAR(new_velocity, 9.9019, 0.0001);
```

#### ✅ Solución Correcta:
```cpp
// Usar la API real con PhysicsState, Vector3, fuerzas
PhysicsState initial_state;
initial_state.position = Vector3(0, 0, 0);
initial_state.velocity = Vector3(0, 0, 0);
initial_state.mass = 1.0;

Vector3 force(0, 10.0, 0);  // 10 N hacia arriba
Vector3 torque(0, 0, 0);
double dt = 0.01;

PhysicsState new_state = integrator->integrate(initial_state, force, torque, dt);

// Verificar resultados físicos reales
EXPECT_NEAR(new_state.velocity.y, 0.1, 0.001);
```

---

### 7. **Manejo de Singletons en Tests**

#### ⚠️ Problema con Singletons:
Los singletons mantienen estado entre tests, lo que puede causar interferencia.

#### ✅ Solución:
```cpp
class TimeManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Resetear singleton antes de cada test
        auto& tm = TimeManager::getInstance();
        tm.reset();
    }

    void TearDown() override {
        // Resetear después de cada test
        auto& tm = TimeManager::getInstance();
        tm.reset();
    }
};
```

---

## 📊 Comparación: Antes vs Después

| Aspecto | ❌ Antes | ✅ Después |
|---------|---------|-----------|
| **Compilación** | Falla (constructores privados) | ✅ Compila |
| **Namespace** | Sin `using namespace MoLab` | ✅ Correcto |
| **Excepciones** | Espera excepciones inexistentes | ✅ Verifica retorno `bool` |
| **Métodos** | Usa métodos que no existen | ✅ Usa API real |
| **Física** | Tests simplistas sin API | ✅ Tests con `PhysicsState`, `Vector3` |
| **Singletons** | Instanciación directa | ✅ `getInstance()` |
| **Estado entre tests** | Sin reset | ✅ Reset en `SetUp/TearDown` |
| **Total de tests** | 16 (no funcionales) | 33 (funcionales) |

---

## 🎓 Lecciones Aprendidas

### 1. **Siempre revisar el código fuente antes de escribir tests**
- No asumas la API sin verificarla
- Lee los headers (.h) para conocer métodos públicos
- Verifica patrones de diseño (Singleton, Factory, etc.)

### 2. **Entender el manejo de errores**
- Algunos proyectos usan excepciones
- Otros usan códigos de retorno (`bool`, `int`)
- MoLab usa retornos `bool` + valores por defecto

### 3. **Respetar la arquitectura existente**
- Si usa Singletons, usa `getInstance()`
- Si usa namespaces, inclúyelos
- Si usa structs para configuración, accede a través de ellos

### 4. **Tests deben ser independientes**
- Cada test debe poder ejecutarse solo
- Usar `SetUp()` y `TearDown()` para estado limpio
- Resetear singletons entre tests

### 5. **Tests deben ser realistas**
- Usar la API pública real
- No simplificar tanto que no pruebe nada
- Verificar comportamiento, no implementación

---

## 🚀 Cómo Escribir Buenos Tests para MoLab

### Template Recomendado:

```cpp
#include <gtest/gtest.h>
#include "TuClase.h"

using namespace MoLab;  // ✅ Importante

class TuClaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Si es singleton:
        auto& instance = TuClase::getInstance();
        instance.reset();  // Si tiene método reset
        
        // Si NO es singleton:
        objeto = new TuClase();
    }

    void TearDown() override {
        // Limpiar recursos
        if (objeto) delete objeto;
    }

    TuClase* objeto = nullptr;
};

TEST_F(TuClaseTest, NombreDescriptivo) {
    // Arrange (preparar)
    auto& instance = TuClase::getInstance();  // Si es singleton
    
    // Act (actuar)
    bool result = instance.metodo();
    
    // Assert (verificar)
    EXPECT_TRUE(result);
}
```

---

## 📚 Recursos Adicionales

### Google Test Macros Útiles:
```cpp
// Comparaciones básicas
EXPECT_EQ(a, b);      // a == b
EXPECT_NE(a, b);      // a != b
EXPECT_TRUE(cond);
EXPECT_FALSE(cond);

// Comparaciones numéricas
EXPECT_LT(a, b);      // a < b
EXPECT_GT(a, b);      // a > b
EXPECT_NEAR(a, b, epsilon);  // |a - b| <= epsilon

// Punto flotante
EXPECT_DOUBLE_EQ(a, b);
EXPECT_FLOAT_EQ(a, b);

// Strings
EXPECT_STREQ(str1, str2);
EXPECT_STRNE(str1, str2);

// Excepciones (cuando aplique)
EXPECT_THROW(statement, exception_type);
EXPECT_NO_THROW(statement);
```

### Patrón AAA (Arrange-Act-Assert):
```cpp
TEST_F(MiTest, DescripcionDelTest) {
    // Arrange: Preparar datos y estado
    auto& manager = ConfigManager::getInstance();
    std::string config_file = "test.json";
    
    // Act: Ejecutar la acción a probar
    bool result = manager.loadConfig(config_file);
    
    // Assert: Verificar resultados
    EXPECT_TRUE(result);
    EXPECT_GT(manager.getSimulationConfig().time_step, 0.0);
}
```

---

## ✅ Checklist para Nuevos Tests

Antes de escribir tests, verifica:

- [ ] ¿La clase usa Singleton? → Usar `getInstance()`
- [ ] ¿Está en un namespace? → Agregar `using namespace`
- [ ] ¿Qué métodos públicos tiene? → Revisar el `.h`
- [ ] ¿Cómo maneja errores? → Excepciones vs códigos de retorno
- [ ] ¿Necesita estado inicial? → Configurar en `SetUp()`
- [ ] ¿Necesita limpieza? → Implementar `TearDown()`
- [ ] ¿Los tests son independientes? → Cada test debe poder ejecutarse solo
- [ ] ¿Uso la API real? → No simplificar demasiado

---

## 🎯 Resultado Final

**Tests corregidos:**
- ✅ **33 tests unitarios** funcionales
- ✅ Compilan correctamente
- ✅ Usan la API real del proyecto
- ✅ Siguen mejores prácticas de Google Test
- ✅ Son independientes y repetibles
- ✅ Cubren casos reales de uso

**Próximos pasos:**
1. Ejecutar tests localmente: `ctest --output-on-failure`
2. Verificar cobertura: `lcov` + `genhtml`
3. Ampliar tests para otros componentes (`SimulationEngine`, `PluginManager`, etc.)
