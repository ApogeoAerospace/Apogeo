# Guía de Desarrollo de Plugins para MoLab

## Introducción

Esta guía proporciona instrucciones completas para desarrollar plugins personalizados para el sistema de simulación aeroespacial MoLab. Los plugins permiten extender las capacidades de simulación con modelos físicos específicos, algoritmos de control, o funcionalidades especializadas.

## Conceptos Fundamentales

### **¿Qué es un Plugin de MoLab?**
Un plugin de MoLab es una biblioteca dinámica (.dll/.dylib/.so) que implementa la API estándar de plugins y proporciona funcionalidad específica para la simulación aeroespacial.

### **Tipos de Plugins**

#### **Tipo 0: Modificadores de Estado Secuenciales**
- **Propósito**: Modificar el estado de simulación de manera ordenada
- **Ejecución**: Secuencial, uno después del otro
- **Acceso**: Lectura y escritura completa del buffer de estado
- **Casos de uso**: Configuración, logging, modificación de condiciones

#### **Tipo 1: Calculadores de Física Paralelos**
- **Propósito**: Calcular fuerzas y torques que afectan la dinámica
- **Ejecución**: Paralelo, thread-safe
- **Acceso**: Solo lectura del buffer de estado
- **Casos de uso**: Aerodinámica, propulsión, estructuras, ambiente

## Configuración del Entorno de Desarrollo

### **Requisitos Previos**
```bash
# Herramientas necesarias
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2019+)
- CMake 3.15+
- FlatBuffers library
- nlohmann/json library (opcional para configuración)

# Dependencias del sistema
- Windows: Visual Studio Build Tools
- macOS: Xcode Command Line Tools
- Linux: build-essential
```

### **Estructura de Proyecto Recomendada**
```
mi_plugin/
├── CMakeLists.txt
├── include/
│   └── mi_plugin.h
├── src/
│   └── mi_plugin.cpp
├── config/
│   └── mi_plugin_config.json
└── tests/
    └── test_mi_plugin.cpp
```

### **CMakeLists.txt Base**
```cmake
cmake_minimum_required(VERSION 3.15)
project(mi_plugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar dependencias
find_package(flatbuffers REQUIRED)

# Incluir headers de MoLab
include_directories(${MOLAB_SOURCE_DIR}/src/api)
include_directories(${MOLAB_SOURCE_DIR}/build/generated)

# Crear biblioteca dinámica
add_library(mi_plugin SHARED
    src/mi_plugin.cpp
)

# Enlazar con FlatBuffers
target_link_libraries(mi_plugin flatbuffers)

# Configurar propiedades de exportación
set_target_properties(mi_plugin PROPERTIES
    PREFIX "lib"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
)

# Instalar en directorio de plugins
install(TARGETS mi_plugin
    DESTINATION ${MOLAB_SOURCE_DIR}/build/lib
)
```

## Implementación Paso a Paso

### **Paso 1: Crear la Clase Base del Plugin**

```cpp
// include/mi_plugin.h
#ifndef MI_PLUGIN_H
#define MI_PLUGIN_H

#include "plugin_api.h"
#include "state_vector_generated.h"
#include <memory>
#include <string>

class MiPlugin {
public:
    MiPlugin();
    ~MiPlugin();
    
    // Configuración del plugin
    bool configure(const std::string& json_config);
    
    // Función principal de ejecución
    int32_t tick(PluginTickData* data);
    
private:
    // Parámetros configurables
    float mi_parametro1_;
    float mi_parametro2_;
    bool habilitado_;
    
    // Métodos internos
    void inicializar_parametros();
    PluginVector3 calcular_fuerza(const state_vector::GeneralState* state);
    PluginVector3 calcular_torque(const state_vector::GeneralState* state);
    
    // Validación
    bool validar_estado(const state_vector::GeneralState* state);
};

#endif // MI_PLUGIN_H
```

### **Paso 2: Implementar la Lógica del Plugin**

```cpp
// src/mi_plugin.cpp
#include "mi_plugin.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>

MiPlugin::MiPlugin() 
    : mi_parametro1_(1.0f)
    , mi_parametro2_(0.5f)
    , habilitado_(true) {
    inicializar_parametros();
}

MiPlugin::~MiPlugin() {
    // Limpieza de recursos si es necesario
}

void MiPlugin::inicializar_parametros() {
    // Configuración por defecto
    mi_parametro1_ = 1.0f;
    mi_parametro2_ = 0.5f;
    habilitado_ = true;
}

bool MiPlugin::configure(const std::string& json_config) {
    try {
        auto config = nlohmann::json::parse(json_config);
        
        // Leer parámetros opcionales
        if (config.contains("mi_parametro1")) {
            mi_parametro1_ = config["mi_parametro1"].get<float>();
        }
        
        if (config.contains("mi_parametro2")) {
            mi_parametro2_ = config["mi_parametro2"].get<float>();
        }
        
        if (config.contains("habilitado")) {
            habilitado_ = config["habilitado"].get<bool>();
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error configurando plugin: " << e.what() << std::endl;
        return false;
    }
}

int32_t MiPlugin::tick(PluginTickData* data) {
    // Validación de entrada
    if (!data || !data->state_buffer) {
        return -1;
    }
    
    if (!habilitado_) {
        return 0; // Plugin deshabilitado
    }
    
    // Deserializar estado
    auto state = state_vector::GetGeneralState(data->state_buffer);
    
    // Validar estado
    if (!validar_estado(state)) {
        return -2;
    }
    
    // Calcular fuerzas y torques
    PluginVector3 fuerza = calcular_fuerza(state);
    PluginVector3 torque = calcular_torque(state);
    
    // Escribir resultados (solo para plugins tipo 1)
    if (data->output_force) {
        data->output_force->x = fuerza.x;
        data->output_force->y = fuerza.y;
        data->output_force->z = fuerza.z;
    }
    
    if (data->output_torque) {
        data->output_torque->x = torque.x;
        data->output_torque->y = torque.y;
        data->output_torque->z = torque.z;
    }
    
    return 0; // Éxito
}

PluginVector3 MiPlugin::calcular_fuerza(const state_vector::GeneralState* state) {
    PluginVector3 fuerza = {0.0f, 0.0f, 0.0f};
    
    // Ejemplo: Fuerza proporcional a la velocidad
    auto velocidad = state->velocity();
    
    fuerza.x = -mi_parametro1_ * velocidad->x();
    fuerza.y = -mi_parametro1_ * velocidad->y();
    fuerza.z = -mi_parametro2_ * velocidad->z();
    
    return fuerza;
}

PluginVector3 MiPlugin::calcular_torque(const state_vector::GeneralState* state) {
    PluginVector3 torque = {0.0f, 0.0f, 0.0f};
    
    // Ejemplo: Torque de estabilización
    auto orientacion = state->orientation();
    
    // Calcular torque correctivo simple
    torque.x = -0.1f * orientacion->x();
    torque.y = -0.1f * orientacion->y();
    torque.z = -0.1f * orientacion->z();
    
    return torque;
}

bool MiPlugin::validar_estado(const state_vector::GeneralState* state) {
    if (!state) return false;
    
    // Validar que los punteros no sean nulos
    if (!state->position() || !state->velocity()) {
        return false;
    }
    
    // Validar rangos razonables
    auto pos = state->position();
    if (std::isnan(pos->x()) || std::isnan(pos->y()) || std::isnan(pos->z())) {
        return false;
    }
    
    return true;
}
```

### **Paso 3: Implementar la Interfaz C de la API**

```cpp
// Agregar al final de src/mi_plugin.cpp

extern "C" {
    
PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    try {
        auto* instance = new MiPlugin();
        return reinterpret_cast<PluginHandle>(instance);
    } catch (const std::exception& e) {
        return nullptr;
    }
}

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle) return -1;
    
    try {
        auto* plugin = reinterpret_cast<MiPlugin*>(handle);
        return plugin->tick(data);
    } catch (const std::exception& e) {
        return -1;
    }
}

PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (handle) {
        auto* plugin = reinterpret_cast<MiPlugin*>(handle);
        delete plugin;
    }
}

PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params) {
    if (!handle || !json_params) return -1;
    
    try {
        auto* plugin = reinterpret_cast<MiPlugin*>(handle);
        std::string config_str(json_params);
        return plugin->configure(config_str) ? 0 : -1;
    } catch (const std::exception& e) {
        return -1;
    }
}

} // extern "C"
```

## Ejemplos de Plugins Especializados

### **Plugin de Aerodinámica Avanzado**

```cpp
class AerodynamicsPlugin {
private:
    float drag_coefficient_ = 0.3f;
    float lift_coefficient_ = 0.1f;
    float reference_area_ = 10.0f;
    bool enable_compressibility_ = true;
    
public:
    PluginVector3 calcular_fuerza(const state_vector::GeneralState* state) {
        auto velocity = state->velocity();
        float speed = std::sqrt(velocity->x() * velocity->x() + 
                               velocity->y() * velocity->y() + 
                               velocity->z() * velocity->z());
        
        if (speed < 0.1f) return {0.0f, 0.0f, 0.0f};
        
        float density = state->atm_density();
        float dynamic_pressure = 0.5f * density * speed * speed;
        
        // Calcular número Mach
        float temperature = state->atm_temperature();
        float sound_speed = std::sqrt(1.4f * 287.0f * temperature);
        float mach = speed / sound_speed;
        
        // Ajustar coeficientes por compresibilidad
        float cd_effective = drag_coefficient_;
        if (enable_compressibility_ && mach > 0.8f) {
            cd_effective *= (1.0f + 2.0f * (mach - 0.8f));
        }
        
        // Calcular fuerzas
        float drag_magnitude = cd_effective * reference_area_ * dynamic_pressure;
        float lift_magnitude = lift_coefficient_ * reference_area_ * dynamic_pressure;
        
        // Aplicar en direcciones apropiadas
        PluginVector3 drag_force;
        drag_force.x = -drag_magnitude * velocity->x() / speed;
        drag_force.y = -drag_magnitude * velocity->y() / speed;
        drag_force.z = -drag_magnitude * velocity->z() / speed + lift_magnitude;
        
        return drag_force;
    }
};
```

### **Plugin de Propulsión con Throttle**

```cpp
class PropulsionPlugin {
private:
    float max_thrust_ = 1000000.0f; // 1MN
    float specific_impulse_ = 300.0f; // segundos
    float throttle_setting_ = 1.0f;
    bool engine_on_ = true;
    
public:
    PluginVector3 calcular_fuerza(const state_vector::GeneralState* state) {
        if (!engine_on_ || throttle_setting_ <= 0.0f) {
            return {0.0f, 0.0f, 0.0f};
        }
        
        // Compensación por altitud (presión atmosférica)
        float pressure = state->atm_pressure();
        float sea_level_pressure = 101325.0f;
        float altitude_factor = std::min(1.0f, pressure / sea_level_pressure);
        
        // Thrust efectivo
        float effective_thrust = max_thrust_ * throttle_setting_ * 
                               (0.8f + 0.2f * altitude_factor);
        
        // Aplicar en dirección Z positiva (hacia arriba)
        return {0.0f, 0.0f, effective_thrust};
    }
    
    float calcular_consumo_combustible(float dt) {
        if (!engine_on_ || throttle_setting_ <= 0.0f) {
            return 0.0f;
        }
        
        float thrust = max_thrust_ * throttle_setting_;
        float mass_flow = thrust / (specific_impulse_ * 9.81f);
        return mass_flow * dt;
    }
};
```

## Configuración y Integración

### **Archivo de Configuración JSON**

```json
{
  "simulation": {
    "time_step": 0.1,
    "duration": 300.0
  },
  "plugins": [
    {
      "name": "mi_plugin",
      "type": 1,
      "library_path": "build/lib/mi_plugin",
      "parameters": {
        "mi_parametro1": 2.5,
        "mi_parametro2": 1.0,
        "habilitado": true
      }
    }
  ]
}
```

### **Compilación del Plugin**

```bash
# Crear directorio de build
mkdir build && cd build

# Configurar con CMake
cmake .. -DMOLAB_SOURCE_DIR=/path/to/molab

# Compilar
make -j4

# Instalar en directorio de plugins de MoLab
make install
```

## Pruebas y Validación

### **Pruebas Unitarias**

```cpp
// tests/test_mi_plugin.cpp
#include <gtest/gtest.h>
#include "mi_plugin.h"

class MiPluginTest : public ::testing::Test {
protected:
    void SetUp() override {
        plugin = std::make_unique<MiPlugin>();
    }
    
    std::unique_ptr<MiPlugin> plugin;
};

TEST_F(MiPluginTest, ConfiguracionBasica) {
    std::string config = R"({
        "mi_parametro1": 3.0,
        "mi_parametro2": 1.5,
        "habilitado": true
    })";
    
    EXPECT_TRUE(plugin->configure(config));
}

TEST_F(MiPluginTest, CalculoFuerzas) {
    // Crear estado de prueba
    flatbuffers::FlatBufferBuilder builder;
    auto pos = state_vector::Vec3(0, 0, 1000);
    auto vel = state_vector::Vec3(100, 0, 0);
    auto state = state_vector::CreateGeneralState(builder, &pos, &vel);
    builder.Finish(state);
    
    // Configurar datos de tick
    PluginTickData data;
    data.state_buffer = builder.GetBufferPointer();
    data.buffer_size = builder.GetSize();
    
    PluginVector3 force_output;
    data.output_force = &force_output;
    
    // Ejecutar plugin
    int32_t result = plugin->tick(&data);
    
    EXPECT_EQ(result, 0);
    EXPECT_LT(force_output.x, 0); // Fuerza opuesta a velocidad
}
```

### **Validación en Simulación Real**

```cpp
// Crear configuración de prueba
{
  "simulation": {
    "time_step": 0.1,
    "duration": 10.0,
    "log_level": "DEBUG"
  },
  "initial_state": {
    "position": [0, 0, 1000],
    "velocity": [100, 0, 0],
    "mass": 1000
  },
  "plugins": [
    {
      "name": "mi_plugin",
      "type": 1,
      "library_path": "build/lib/mi_plugin",
      "parameters": {
        "mi_parametro1": 0.1,
        "habilitado": true
      }
    }
  ]
}
```

## Mejores Prácticas

### **Gestión de Errores Robusta**

```cpp
int32_t MiPlugin::tick(PluginTickData* data) {
    try {
        // Validación exhaustiva
        if (!data) return -1;
        if (!data->state_buffer) return -2;
        if (data->buffer_size == 0) return -3;
        
        // Deserialización segura
        auto state = state_vector::GetGeneralState(data->state_buffer);
        if (!state) return -4;
        
        // Validación de estado
        if (!validar_estado(state)) return -5;
        
        // Cálculos principales
        // ...
        
        return 0;
    } catch (const std::exception& e) {
        // Log del error si es posible
        return -99;
    }
}
```

### **Optimización de Rendimiento**

```cpp
class OptimizedPlugin {
private:
    // Cache para cálculos costosos
    mutable float cached_drag_coeff_ = -1.0f;
    mutable float last_mach_number_ = -1.0f;
    
public:
    float get_drag_coefficient(float mach) const {
        if (std::abs(mach - last_mach_number_) > 0.01f) {
            // Recalcular solo si Mach cambió significativamente
            cached_drag_coeff_ = calculate_drag_coefficient(mach);
            last_mach_number_ = mach;
        }
        return cached_drag_coeff_;
    }
    
    // Evitar cálculos innecesarios
    PluginVector3 calcular_fuerza(const state_vector::GeneralState* state) {
        auto velocity = state->velocity();
        float speed_squared = velocity->x() * velocity->x() + 
                             velocity->y() * velocity->y() + 
                             velocity->z() * velocity->z();
        
        if (speed_squared < 0.01f) {
            return {0.0f, 0.0f, 0.0f}; // Velocidad despreciable
        }
        
        // Continuar con cálculos...
    }
};
```

### **Thread Safety**

```cpp
class ThreadSafePlugin {
private:
    mutable std::mutex calculation_mutex_;
    std::atomic<bool> enabled_{true};
    
public:
    int32_t tick(PluginTickData* data) {
        if (!enabled_.load()) return 0;
        
        std::lock_guard<std::mutex> lock(calculation_mutex_);
        
        // Cálculos thread-safe aquí
        return 0;
    }
    
    void set_enabled(bool enabled) {
        enabled_.store(enabled);
    }
};
```

## Depuración y Troubleshooting

### **Técnicas de Depuración**

```cpp
// Logging condicional
#ifdef DEBUG_MI_PLUGIN
#define DEBUG_LOG(msg) std::cout << "[MiPlugin] " << msg << std::endl
#else
#define DEBUG_LOG(msg)
#endif

int32_t MiPlugin::tick(PluginTickData* data) {
    DEBUG_LOG("Iniciando tick del plugin");
    
    auto state = state_vector::GetGeneralState(data->state_buffer);
    auto pos = state->position();
    
    DEBUG_LOG("Posición: (" << pos->x() << ", " << pos->y() << ", " << pos->z() << ")");
    
    // Continuar con lógica...
}
```

### **Validación de Resultados**

```cpp
bool MiPlugin::validar_resultados(const PluginVector3& fuerza) {
    // Verificar NaN
    if (std::isnan(fuerza.x) || std::isnan(fuerza.y) || std::isnan(fuerza.z)) {
        return false;
    }
    
    // Verificar magnitud razonable
    float magnitud = std::sqrt(fuerza.x * fuerza.x + 
                              fuerza.y * fuerza.y + 
                              fuerza.z * fuerza.z);
    
    if (magnitud > 1e8f) { // 100 MN límite superior
        return false;
    }
    
    return true;
}
```

## Distribución del Plugin

### **Empaquetado**

```cmake
# Crear paquete instalable
set(CPACK_PACKAGE_NAME "MiPlugin")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_DESCRIPTION "Plugin personalizado para MoLab")

include(CPack)
```

### **Documentación del Plugin**

```markdown
# Mi Plugin para MoLab

## Descripción
Plugin que implementa [funcionalidad específica].

## Parámetros de Configuración
- `mi_parametro1`: Descripción del parámetro (float, default: 1.0)
- `mi_parametro2`: Descripción del parámetro (float, default: 0.5)
- `habilitado`: Habilitar/deshabilitar plugin (bool, default: true)

## Ejemplo de Uso
```json
{
  "name": "mi_plugin",
  "type": 1,
  "library_path": "build/lib/mi_plugin",
  "parameters": {
    "mi_parametro1": 2.0,
    "habilitado": true
  }
}
```

Esta guía proporciona una base completa para desarrollar plugins profesionales para MoLab, desde la configuración inicial hasta la distribución final.
