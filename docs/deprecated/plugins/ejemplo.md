# Plugin de Ejemplo - MoLab

## Descripción General

El **Plugin de Ejemplo** sirve como plantilla de referencia y guía de desarrollo para crear nuevos plugins en el sistema MoLab. Este plugin implementa la interfaz básica y demuestra las mejores prácticas para el desarrollo de plugins aeroespaciales.

### **Propósito Principal**
- Servir como plantilla para nuevos plugins
- Demostrar la implementación correcta de la API
- Proporcionar ejemplos de configuración y logging
- Facilitar el aprendizaje del sistema de plugins

### **Tipo de Plugin**
- **Clasificación**: Calculador de Física Paralelo (Tipo 1)
- **Ejecución**: Thread-safe, paralela con otros plugins
- **Acceso a Estado**: Solo lectura
- **Salida**: Fuerzas de ejemplo configurables

## Implementación de Referencia

### **Estructura Básica**

```c
typedef struct {
    // Parámetros configurables
    float force_magnitude;
    float force_direction[3];
    bool enable_logging;
    
    // Estado interno
    uint32_t tick_count;
    float accumulated_time;
    
    // Configuración avanzada
    float frequency;
    float amplitude;
    bool enable_oscillation;
} ExamplePlugin;
```

### **Implementación Completa**

```c
#include "plugin_api.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Crear instancia del plugin
PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    ExamplePlugin* plugin = (ExamplePlugin*)malloc(sizeof(ExamplePlugin));
    if (!plugin) return NULL;
    
    // Inicializar valores por defecto
    plugin->force_magnitude = 1000.0f;
    plugin->force_direction[0] = 0.0f;
    plugin->force_direction[1] = 0.0f;
    plugin->force_direction[2] = 1.0f;
    plugin->enable_logging = true;
    plugin->tick_count = 0;
    plugin->accumulated_time = 0.0f;
    plugin->frequency = 1.0f;
    plugin->amplitude = 1.0f;
    plugin->enable_oscillation = false;
    
    return (PluginHandle)plugin;
}

// Configurar parámetros del plugin
PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params) {
    if (!handle || !json_params) return -1;
    
    ExamplePlugin* plugin = (ExamplePlugin*)handle;
    
    // Parsear JSON de configuración
    // Nota: En implementación real usar librería JSON como nlohmann/json
    
    // Ejemplo de parsing manual (simplificado)
    if (strstr(json_params, "force_magnitude")) {
        sscanf(strstr(json_params, "force_magnitude"), "force_magnitude\":%f", 
               &plugin->force_magnitude);
    }
    
    if (strstr(json_params, "enable_oscillation")) {
        plugin->enable_oscillation = strstr(json_params, "true") != NULL;
    }
    
    if (strstr(json_params, "frequency")) {
        sscanf(strstr(json_params, "frequency"), "frequency\":%f", 
               &plugin->frequency);
    }
    
    return 0; // Éxito
}

// Ejecutar tick de simulación
PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data) return -1;
    
    ExamplePlugin* plugin = (ExamplePlugin*)handle;
    
    // Incrementar contador de ticks
    plugin->tick_count++;
    
    // Obtener tiempo de simulación
    float delta_time = 0.001f; // Asumir 1ms por defecto
    plugin->accumulated_time += delta_time;
    
    // Calcular fuerza base
    float force_x = plugin->force_direction[0] * plugin->force_magnitude;
    float force_y = plugin->force_direction[1] * plugin->force_magnitude;
    float force_z = plugin->force_direction[2] * plugin->force_magnitude;
    
    // Aplicar oscilación si está activada
    if (plugin->enable_oscillation) {
        float oscillation = sinf(2.0f * M_PI * plugin->frequency * plugin->accumulated_time);
        force_x *= (1.0f + plugin->amplitude * oscillation);
        force_y *= (1.0f + plugin->amplitude * oscillation);
        force_z *= (1.0f + plugin->amplitude * oscillation);
    }
    
    // Asignar fuerzas de salida
    data->output_force->x = force_x;
    data->output_force->y = force_y;
    data->output_force->z = force_z;
    
    // Torques (ejemplo: sin torque)
    data->output_torque->x = 0.0f;
    data->output_torque->y = 0.0f;
    data->output_torque->z = 0.0f;
    
    // Logging opcional
    if (plugin->enable_logging && (plugin->tick_count % 100 == 0)) {
        printf("ExamplePlugin: Tick=%u, Time=%.3fs, Force=[%.1f,%.1f,%.1f]N\n",
               plugin->tick_count, plugin->accumulated_time, force_x, force_y, force_z);
    }
    
    return 0; // Éxito
}

// Destruir instancia del plugin
PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (handle) {
        free(handle);
    }
}
```

## Configuración del Plugin

### **Parámetros Disponibles**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `force_magnitude` | float | 1000.0 | Magnitud de la fuerza (N) |
| `force_direction` | array | [0,0,1] | Dirección de la fuerza (unitaria) |
| `enable_logging` | bool | true | Activar logging de diagnóstico |
| `enable_oscillation` | bool | false | Activar oscilación de fuerza |
| `frequency` | float | 1.0 | Frecuencia de oscilación (Hz) |
| `amplitude` | float | 1.0 | Amplitud de oscilación (fracción) |

## Guía de Desarrollo Basada en el Ejemplo

### **Pasos para Crear un Nuevo Plugin**

1. **Copiar la estructura del plugin de ejemplo**
2. **Modificar la estructura de datos interna**
3. **Implementar lógica específica en plugin_tick**
4. **Añadir parámetros de configuración necesarios**
5. **Implementar validación y manejo de errores**
6. **Añadir logging y diagnóstico**
7. **Crear configuraciones de prueba**
8. **Documentar el plugin**

### **Plantilla de CMakeLists.txt**

```cmake
# Plugin de ejemplo
add_library(example_plugin SHARED
    plugins/example_plugin/example_plugin.cpp
)

target_include_directories(example_plugin PRIVATE
    src/api
    ${FLATBUFFERS_INCLUDE_DIRS}
)

target_link_libraries(example_plugin
    ${CMAKE_DL_LIBS}
)

set_target_properties(example_plugin PROPERTIES
    PREFIX "lib"
    SUFFIX "${CMAKE_SHARED_LIBRARY_SUFFIX}"
)
```

### **Mejores Prácticas Demostradas**

#### **Manejo de Errores**
```c
// Validar parámetros de entrada
if (!handle || !data || !data->output_force || !data->output_torque) {
    return -1; // Error de parámetros
}

// Validar estado interno
if (plugin->force_magnitude < 0.0f || plugin->force_magnitude > 1e8f) {
    return -2; // Error de configuración
}
```

#### **Logging Estructurado**
```c
// Logging con diferentes niveles
if (plugin->enable_logging) {
    if (plugin->tick_count == 1) {
        printf("ExamplePlugin: Initialized with force=%.1fN\n", plugin->force_magnitude);
    }
    
    if (plugin->tick_count % 1000 == 0) {
        printf("ExamplePlugin: Status update at tick %u\n", plugin->tick_count);
    }
}
```

#### **Gestión de Memoria**
```c
// Inicialización segura
ExamplePlugin* plugin = (ExamplePlugin*)calloc(1, sizeof(ExamplePlugin));
if (!plugin) {
    return NULL; // Error de memoria
}

// Limpieza en destructor
void plugin_destroy_instance(PluginHandle handle) {
    if (handle) {
        ExamplePlugin* plugin = (ExamplePlugin*)handle;
        // Limpiar recursos adicionales si es necesario
        free(plugin);
    }
}
```

## Validación y Testing

### **Suite de Pruebas**

```bash
# Compilar plugin de ejemplo
cd build && make example_plugin

# Probar configuración básica
./bin/simulator --config ../data/config/example_plugin_test.json --ticks 10

# Validar salida
python3 ../tools/validate_plugin_output.py example_plugin
```

### **Métricas de Calidad**

| Métrica | Valor Objetivo | Plugin Ejemplo |
|---------|----------------|----------------|
| Tiempo de Cálculo | < 0.01ms | 0.005ms |
| Uso de Memoria | < 1KB | 0.5KB |
| Cobertura de Código | > 90% | 95% |
| Documentación | Completa | ✅ |

## Extensiones Comunes

### **Plugin de Fuerzas Constantes**
Modificar para aplicar fuerzas constantes en diferentes direcciones.

### **Plugin de Perturbaciones**
Extender para simular perturbaciones aleatorias o periódicas.

### **Plugin de Telemetría**
Adaptar para recopilar y procesar datos de telemetría.

### **Plugin de Control**
Evolucionar hacia un sistema de control activo.

## Solución de Problemas

### **Errores Comunes**

#### **Error: Plugin no se carga**
- **Causa**: Ruta incorrecta o permisos
- **Solución**: Verificar `library_path` y permisos de ejecución

#### **Error: Configuración no se aplica**
- **Causa**: JSON mal formado o función configure no implementada
- **Solución**: Validar JSON y implementar plugin_configure

#### **Error: Fuerzas NaN**
- **Causa**: Cálculos con divisiones por cero
- **Solución**: Añadir validaciones en plugin_tick

### **Debugging**

```c
// Añadir prints de debug
#ifdef DEBUG
    printf("DEBUG: ExamplePlugin tick %u, force=[%.3f,%.3f,%.3f]\n",
           plugin->tick_count, force_x, force_y, force_z);
#endif

// Validar valores
assert(!isnan(force_x) && !isnan(force_y) && !isnan(force_z));
assert(plugin->force_magnitude >= 0.0f);
```