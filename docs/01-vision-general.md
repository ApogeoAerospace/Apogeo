# Visión General del Proyecto MoLab

## ¿Qué es MoLab?

MoLab es un sistema sofisticado de simulación aeroespacial diseñado para la simulación realista de naves espaciales y cohetes. Proporciona una arquitectura modular basada en plugins que permite el modelado preciso de varios fenómenos aeroespaciales incluyendo aerodinámica, propulsión, dinámica estructural y efectos ambientales.

## Características Principales

### **Simulación de Física Realista**
- Modelado de física aeroespacial de alta fidelidad
- Múltiples métodos de integración numérica (Euler, Runge-Kutta 4, Verlet)
- Cálculos de fuerza y torque en tiempo real
- Seguimiento preciso de masa y consumo de combustible

### **Arquitectura Basada en Plugins**
- Diseño modular con plugins intercambiables
- Dos tipos de plugins: Modificadores de Estado Secuenciales y Calculadores de Física Paralelos
- Ejecución de plugins thread-safe
- Carga y configuración dinámica de plugins

### **Sistema de Interfaz Universal**
- **Interfaz Web**: GUI basada en navegador accesible desde cualquier lugar (SSH, Docker, Cloud)
- **GUI de Escritorio**: Interfaz en Tkinter para entornos de escritorio

### **Visualización Avanzada**
- Análisis de trayectoria en tiempo real
- Visualización multi-gráfico (Posición, Velocidad, Altitud, Velocidad)
- Métricas y alertas específicas
- Sistema de plantillas de misión con escenarios predefinidos

###  **Sistema de Configuración Profesional**
- Gestión de configuración basada en JSON
- Plantillas de misión (Suborbital, Orbital, Aterrizaje, Atmosférico)
- Validación de parámetros y manejo de errores

## Plugins

### **Plugin de Aerodinámica**
- Cálculos de fuerzas de arrastre y sustentación
- Efectos atmosféricos dependientes de la altitud

### **Plugin de Propulsión**
- Modelado de empuje multi-motor
- Consumo de combustible e impulso específico (ISP)
- Gestión de perfiles de aceleración
- Compensación de altitud y vectorización de empuje

### **Plugin de Estructuras**
- Seguimiento de masa y centro de gravedad
- Cálculos de momentos de inercia
- Análisis de factores de carga
- Amortiguación estructural y efectos de vibración

### **Plugin de Ambiente**
- Modelo de atmósfera estándar ISA
- Efectos de viento y turbulencia
- Variación gravitacional con altitud
- Efectos de Coriolis y centrífugos

## Aplicaciones Objetivo

### **Vuelos Suborbitales**
- Misiones estilo New Shepard
- Análisis de trayectoria parabólica
- Simulación de microgravedad
- Modelado de vuelos espaciales turísticos

### **Misiones Orbitales**
- Lanzamientos estilo Falcon 9
- Separación multi-etapa
- Análisis de inserción orbital
- Simulación de despliegue de satélites

### **Simulaciones de Aterrizaje**
- Guía de descenso propulsado
- Análisis de aterrizaje propulsivo
- Modelado de control de aletas de rejilla
- Escenarios de aterrizaje de precisión

### **Pruebas Atmosféricas**
- Misiones de globos de gran altitud
- Vuelos de investigación atmosférica
- Experimentos estratosféricos

## Especificaciones Técnicas

### **Tecnologías Principales**
- **Lenguaje**: C++17 con características modernas
- **Sistema de Compilación**: CMake con soporte multiplataforma
- **Serialización**: FlatBuffers para gestión de estado de alto rendimiento
- **Configuración**: JSON con biblioteca nlohmann_json
- **Threading**: Ejecución de plugins thread-safe con protección mutex

### **Plataformas Soportadas**
- **Windows**: Soporte nativo de DLL
- **macOS**: Soporte de biblioteca dinámica (.dylib)
- **Linux**: Soporte de objeto compartido (.so)
- **Docker**: Despliegue listo para contenedores
- **Cloud**: Compatible con SSH y acceso remoto

### **Características de Rendimiento**
- **Capacidad en tiempo real**: Ejecución de tick sub-milisegundo
- **Escalable**: Soporta múltiples plugins concurrentes
- **Eficiente en memoria**: Serialización zero-copy de FlatBuffers
- **Thread-safe**: Ejecución de plugins paralelos sin condiciones de carrera

## Comenzando

1. **Instalación**: Sigue la [Guía de Instalación](./05-instalacion.md)
2. **Primera Simulación**: Usa la [Guía de Usuario](./06-guia-usuario.md)
3. **Desarrollo de Plugins**: Ver [Guía de Desarrollo de Plugins](./04-desarrollo-plugins.md)
4. **Configuración**: Consulta [Referencia de Configuración](./07-configuracion.md)

## Comunidad y Soporte
- **Soporte**:
- **samuel.bedoyao@udea.edu.co**
- **johandres123@hotmail.com**
