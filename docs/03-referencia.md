# Referencia de API de MoLab

## Visión General de la API

La API de MoLab está diseñada para proporcionar una interfaz consistente y eficiente para el desarrollo de plugins de simulación aeroespacial. La API utiliza una interfaz C pura para garantizar la compatibilidad multiplataforma y la interoperabilidad entre diferentes compiladores.

## Estructuras de Datos Principales

### **PluginVector3**
Estructura para representar vectores de 3 componentes (fuerzas, torques, posiciones, velocidades).

```c
typedef struct {
    float x;  // Componente X
    float y;  // Componente Y  
    float z;  // Componente Z
} PluginVector3;
```

**Uso común:**
- Fuerzas: Newtons (N)
- Torques: Newton-metros (N⋅m)
- Posiciones: Metros (m)
- Velocidades: Metros por segundo (m/s)
- Aceleraciones: Metros por segundo cuadrado (m/s²)

### **PluginHandle**
Handle opaco para manejar instancias de plugins de manera segura.

```c
typedef struct PluginInstance* PluginHandle;
```

**Características:**
- Encapsulación de datos internos del plugin
- Gestión segura de memoria
- Prevención de acceso directo a estructuras internas
- Compatibilidad con diferentes implementaciones

### **PluginTickData**
Estructura principal para el intercambio de datos durante la ejecución de plugins.

```c
typedef struct {
    // --- ENTRADA/SALIDA ---
    uint8_t* state_buffer;    // Buffer de estado central (FlatBuffers)
    uint32_t buffer_size;     // Tamaño del buffer en bytes
    
    // --- SALIDA DE FUERZAS ---
    PluginVector3* output_force;   // Fuerza calculada por el plugin
    PluginVector3* output_torque;  // Torque calculado por el plugin
} PluginTickData;
```

**Campos detallados:**

- **`state_buffer`**: Puntero al buffer de estado serializado en FlatBuffers
  - Contiene toda la información del estado actual de la simulación
  - Formato binario para máximo rendimiento
  - Acceso de solo lectura para plugins paralelos

- **`buffer_size`**: Tamaño del buffer de estado
  - Usado para validación de límites
  - Previene accesos fuera de memoria

- **`output_force`**: Puntero para escribir la fuerza calculada
  - Solo válido para plugins de tipo PARALLEL_PHYSICS_CALCULATOR
  - NULL para plugins que no calculan fuerzas
  - Coordenadas en sistema de referencia del vehículo

- **`output_torque`**: Puntero para escribir el torque calculado
  - Solo válido para plugins de tipo PARALLEL_PHYSICS_CALCULATOR
  - NULL para plugins que no calculan torques
  - Coordenadas en sistema de referencia del vehículo

## Funciones de la API

### **plugin_create_instance()**
Crea una nueva instancia del plugin.

```c
PLUGIN_EXPORT PluginHandle plugin_create_instance();
```

**Propósito:**
- Inicializar estructuras de datos internas del plugin
- Asignar memoria necesaria
- Configurar estado inicial del plugin

**Retorno:**
- `PluginHandle`: Handle válido si la creación fue exitosa
- `NULL`: Si ocurrió un error durante la creación

**Ejemplo de implementación:**
```cpp
extern "C" PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    try {
        auto* instance = new AerodynamicsPlugin();
        return reinterpret_cast<PluginHandle>(instance);
    } catch (const std::exception& e) {
        return nullptr;
    }
}
```

### **plugin_tick()**
Ejecuta un tick de simulación para el plugin.

```c
PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data);
```

**Parámetros:**
- `handle`: Handle de la instancia del plugin
- `data`: Estructura con datos de entrada y salida

**Retorno:**
- `0`: Ejecución exitosa
- `-1`: Error durante la ejecución
- `> 0`: Códigos de advertencia específicos del plugin

**Flujo de ejecución:**
1. Validar parámetros de entrada
2. Deserializar estado desde `state_buffer`
3. Realizar cálculos específicos del plugin
4. Escribir resultados en `output_force` y `output_torque` (si aplica)
5. Retornar código de estado

**Ejemplo de implementación:**
```cpp
extern "C" PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data || !data->state_buffer) {
        return -1;
    }
    
    auto* plugin = reinterpret_cast<AerodynamicsPlugin*>(handle);
    
    // Deserializar estado
    auto state = GetGeneralState(data->state_buffer);
    
    // Calcular fuerzas aerodinámicas
    PluginVector3 drag_force = plugin->calculate_drag(state);
    PluginVector3 lift_force = plugin->calculate_lift(state);
    
    // Escribir resultados
    if (data->output_force) {
        data->output_force->x = drag_force.x + lift_force.x;
        data->output_force->y = drag_force.y + lift_force.y;
        data->output_force->z = drag_force.z + lift_force.z;
    }
    
    if (data->output_torque) {
        data->output_torque->x = 0.0f;
        data->output_torque->y = 0.0f;
        data->output_torque->z = 0.0f;
    }
    
    return 0;
}
```

### **plugin_destroy_instance()**
Destruye una instancia del plugin y libera recursos.

```c
PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle);
```

**Parámetros:**
- `handle`: Handle de la instancia a destruir

**Responsabilidades:**
- Liberar toda la memoria asignada
- Cerrar archivos o conexiones abiertas
- Limpiar recursos del sistema
- Invalidar el handle

**Ejemplo de implementación:**
```cpp
extern "C" PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (handle) {
        auto* plugin = reinterpret_cast<AerodynamicsPlugin*>(handle);
        delete plugin;
    }
}
```

### **plugin_configure()** (Opcional)
Configura parámetros del plugin desde JSON.

```c
PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params);
```

**Parámetros:**
- `handle`: Handle de la instancia del plugin
- `json_params`: Cadena JSON con parámetros de configuración

**Retorno:**
- `0`: Configuración exitosa
- `-1`: Error en la configuración
- `> 0`: Advertencias de configuración

**Ejemplo de JSON de configuración:**
```json
{
    "drag_coefficient": 0.3,
    "reference_area": 10.0,
    "enable_compressibility": true,
    "mach_transition": 0.8
}
```

## Acceso al Estado de Simulación

### **Deserialización de FlatBuffers**
El estado de simulación se proporciona como buffer FlatBuffers serializado.

```cpp
// Acceder al estado principal
auto state = GetGeneralState(data->state_buffer);

// Leer posición
auto position = state->position();
float x = position->x();
float y = position->y();
float z = position->z();

// Leer velocidad
auto velocity = state->velocity();
float vx = velocity->x();
float vy = velocity->y();
float vz = velocity->z();

// Leer propiedades atmosféricas
float density = state->atm_density();
float pressure = state->atm_pressure();
float temperature = state->atm_temperature();

// Leer tiempo
float sim_time = state->Time();
float utc_time = state->UTC();
```

### **Campos Disponibles del Estado**
- **Posición**: `position()` - Vector3 en metros
- **Velocidad**: `velocity()` - Vector3 en m/s
- **Orientación**: `orientation()` - Quaternion
- **Masa**: `mass()` - Masa actual en kg
- **Densidad Atmosférica**: `atm_density()` - kg/m³
- **Presión Atmosférica**: `atm_pressure()` - Pa
- **Temperatura Atmosférica**: `atm_temperature()` - K
- **Gravedad**: `gravity()` - Vector3 en m/s²
- **Velocidad del Viento**: `wind_speed()` - Vector3 en m/s
- **Tiempo de Simulación**: `Time()` - Segundos desde inicio
- **Tiempo UTC**: `UTC()` - Segundos desde epoch Unix

## Tipos de Plugins

### **SEQUENTIAL_STATE_MODIFIER (Tipo 0)**
Plugins que modifican el estado de manera secuencial.

**Características:**
- Ejecución en orden específico
- Acceso completo de lectura/escritura al buffer de estado
- No calculan fuerzas/torques
- `output_force` y `output_torque` son NULL

**Casos de uso:**
- Modificación de condiciones ambientales
- Actualización de parámetros de misión
- Logging y monitoreo
- Configuración dinámica

### **PARALLEL_PHYSICS_CALCULATOR (Tipo 1)**
Plugins que calculan fuerzas y torques en paralelo.

**Características:**
- Ejecución paralela thread-safe
- Acceso de solo lectura al buffer de estado
- Calculan fuerzas y torques
- `output_force` y `output_torque` son válidos

**Casos de uso:**
- Cálculos aerodinámicos
- Fuerzas de propulsión
- Efectos estructurales
- Fuerzas ambientales

## Mejores Prácticas

### **Validación de Entrada**
```cpp
extern "C" PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    // Validar parámetros críticos
    if (!handle) return -1;
    if (!data) return -1;
    if (!data->state_buffer) return -1;
    if (data->buffer_size == 0) return -1;
    
    // Continuar con lógica del plugin...
}
```

### **Manejo de Errores**
```cpp
extern "C" PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    try {
        auto* instance = new MyPlugin();
        if (!instance->initialize()) {
            delete instance;
            return nullptr;
        }
        return reinterpret_cast<PluginHandle>(instance);
    } catch (const std::exception& e) {
        // Log error if logging is available
        return nullptr;
    }
}
```

### **Thread Safety**
```cpp
class MyPlugin {
private:
    std::mutex calculation_mutex_;
    
public:
    int32_t tick(PluginTickData* data) {
        std::lock_guard<std::mutex> lock(calculation_mutex_);
        // Cálculos thread-safe aquí
        return 0;
    }
};
```

### **Optimización de Rendimiento**
```cpp
// Evitar cálculos innecesarios
if (data->output_force == nullptr && data->output_torque == nullptr) {
    return 0; // No se requieren cálculos de fuerza
}

// Cachear cálculos costosos
static float cached_drag_coefficient = -1.0f;
if (cached_drag_coefficient < 0.0f) {
    cached_drag_coefficient = calculate_drag_coefficient();
}
```

## Ejemplo Completo de Plugin

```cpp
#include "plugin_api.h"
#include "state_vector_generated.h"
#include <cmath>

class ExamplePlugin {
private:
    float drag_coefficient_ = 0.3f;
    float reference_area_ = 10.0f;
    
public:
    int32_t tick(PluginTickData* data) {
        auto state = GetGeneralState(data->state_buffer);
        
        // Calcular velocidad total
        auto velocity = state->velocity();
        float speed = std::sqrt(velocity->x() * velocity->x() + 
                               velocity->y() * velocity->y() + 
                               velocity->z() * velocity->z());
        
        // Calcular fuerza de arrastre
        float density = state->atm_density();
        float drag_magnitude = 0.5f * density * speed * speed * 
                              drag_coefficient_ * reference_area_;
        
        // Aplicar en dirección opuesta a la velocidad
        if (data->output_force && speed > 0.0f) {
            float drag_factor = -drag_magnitude / speed;
            data->output_force->x = velocity->x() * drag_factor;
            data->output_force->y = velocity->y() * drag_factor;
            data->output_force->z = velocity->z() * drag_factor;
        }
        
        return 0;
    }
};

// Implementación de la API
extern "C" {
    PLUGIN_EXPORT PluginHandle plugin_create_instance() {
        return reinterpret_cast<PluginHandle>(new ExamplePlugin());
    }
    
    PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
        if (!handle || !data) return -1;
        auto* plugin = reinterpret_cast<ExamplePlugin*>(handle);
        return plugin->tick(data);
    }
    
    PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
        if (handle) {
            delete reinterpret_cast<ExamplePlugin*>(handle);
        }
    }
}
```

Esta API proporciona una base sólida y flexible para el desarrollo de plugins de simulación aeroespacial, manteniendo al mismo tiempo la eficiencia y la seguridad necesarias para aplicaciones de alto rendimiento.
