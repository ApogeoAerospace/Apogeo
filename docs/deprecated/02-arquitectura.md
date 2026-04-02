# Arquitectura del Sistema MoLab

## Visión General de la Arquitectura

MoLab está diseñado con una arquitectura modular y extensible que separa las tareas principales en componentes bien definidos. El sistema sigue principios de diseño orientado a objetos y patrones de arquitectura que facilitan el mantenimiento, la extensibilidad y el rendimiento.

## Componentes Principales

### **Motor de Simulación (SimulationEngine)**
El componente central que orquesta toda la simulación.

**Responsabilidades:**
- Inicialización del sistema de simulación
- Gestión del ciclo de vida de la simulación
- Coordinación entre componentes
- Gestión del estado global del sistema

**Archivos principales:**
- `core/SimulationEngine.h`
- `core/SimulationEngine.cpp`

### **Gestor de Plugins (PluginManager)**
Administra el ciclo de vida y la ejecución de todos los plugins.

**Responsabilidades:**
- Carga dinámica de plugins (.dll/.dylib/.so)
- Clasificación de plugins por tipo
- Ejecución thread-safe de plugins
- Gestión de memoria y recursos de plugins
- Acumulación y aplicación de fuerzas/torques

**Archivos principales:**
- `core/PluginManager.h`
- `core/PluginManager.cpp`

### **Cargador de Estado Inicial (InitialStateLoader)**
Maneja la carga y conversión del estado inicial desde archivos JSON.

**Responsabilidades:**
- Parsing de archivos JSON de configuración
- Conversión a formato FlatBuffers
- Validación de datos de entrada
- Inicialización del estado de simulación

**Archivos principales:**
- `core/InitialStateLoader.h`
- `core/InitialStateLoader.cpp`

### **API de Plugins**
Define la interfaz común para todos los plugins del sistema.

**Responsabilidades:**
- Definición de estructuras de datos compartidas
- Interfaz C para compatibilidad multiplataforma
- Gestión de handles opacos
- Definición de tipos de datos (PluginVector3, PluginTickData)

**Archivos principales:**
- `src/api/plugin_api.h`

## Tipos de Plugins

### **Modificadores de Estado Secuenciales (SEQUENTIAL_STATE_MODIFIER)**
**Tipo:** 0
**Ejecución:** Secuencial, uno tras otro
**Propósito:** Modificar el estado del sistema de manera ordenada

**Características:**
- Acceso completo al buffer de estado
- Ejecución en orden específico
- Modificación directa del estado
- Sin cálculo de fuerzas/torques

**Ejemplos:**
- Plugins de configuración
- Modificadores de condiciones iniciales
- Plugins de logging y monitoreo

### **Calculadores de Física Paralelos (PARALLEL_PHYSICS_CALCULATOR)**
**Tipo:** 1
**Ejecución:** Paralelo, thread-safe
**Propósito:** Calcular fuerzas y torques que afectan la dinámica del vehículo

**Características:**
- Ejecución paralela para mejor rendimiento
- Cálculo de fuerzas y torques
- Thread-safe con protección mutex
- Acumulación automática de resultados

**Ejemplos:**
- Plugin de Aerodinámica
- Plugin de Propulsión
- Plugin de Estructuras
- Plugin de Ambiente

## Gestión de Estado

### **Esquema FlatBuffers**
El estado del sistema se define usando FlatBuffers para serialización eficiente.

**Estructura del Estado (`state_vector.fbs`):**

###  **Flujo de Datos**
1. **Inicialización**: JSON → FlatBuffers → Buffer de Estado
2. **Ejecución**: Buffer de Estado → Plugins → Buffer Modificado
3. **Integración**: Fuerzas Acumuladas → Integrador Físico → Nuevo Estado
4. **Salida**: Buffer de Estado → JSON/CSV → Archivos de Resultados

## Patrones de Diseño Implementados

### **Patrón Factory**
- **Ubicación**: Carga de plugins
- **Propósito**: Creación dinámica de instancias de plugins
- **Implementación**: Funciones `plugin_create_instance()`

### **Patrón Strategy**
- **Ubicación**: Tipos de plugins
- **Propósito**: Diferentes estrategias de procesamiento (secuencial vs paralelo)
- **Implementación**: Enum `PluginType`

### **Patrón Observer**
- **Ubicación**: Sistema de eventos de simulación
- **Propósito**: Notificación de cambios de estado
- **Implementación**: Callbacks y eventos de tick

### **Patrón Singleton**
- **Ubicación**: Gestores globales (ConfigManager, TimeManager)
- **Propósito**: Acceso global a configuración y tiempo
- **Implementación**: Instancias únicas thread-safe

## Integración Física

### **Integradores Numéricos**
El sistema soporta múltiples métodos de integración:

1. **Euler Simple**
   - Rápido pero menos preciso
   - Recomendado para pruebas rápidas
   
2. **Runge-Kutta 4to Orden**
   - Balance entre precisión y rendimiento
   - Recomendado para simulaciones de producción
   
3. **Verlet**
   - Excelente para sistemas conservativos
   - Recomendado para simulaciones de larga duración

## Interfaces de Usuario

### **Interfaz Web Universal**
- **Tecnología**: Python Flask + HTML/CSS/JavaScript
- **Puerto**: 8082 (configurable)
- **Características**: Responsive, accesible remotamente, sin dependencias locales

### **GUI de Escritorio**
- **Tecnología**: Python Tkinter
- **Características**: Interfaz rica, controles nativos, análisis integrado

## Extensibilidad

### **Desarrollo de Nuevos Plugins**
1. Implementar interfaz `plugin_api.h`
2. Definir tipo de plugin (secuencial o paralelo)
3. Implementar funciones requeridas:
   - `plugin_create_instance()`
   - `plugin_tick()`
   - `plugin_destroy_instance()`
4. Compilar como biblioteca dinámica
5. Configurar en archivo JSON

Esta arquitectura proporciona una base sólida y extensible para simulaciones aeroespaciales de alta fidelidad, manteniendo al mismo tiempo la flexibilidad necesaria para adaptarse a diferentes casos de uso y requisitos de rendimiento.
