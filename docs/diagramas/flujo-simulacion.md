# Flujo de Simulación - MoLab

## Diagrama de Secuencia Principal

```mermaid
sequenceDiagram
    participant User as 👤 Usuario
    participant Interface as 🌐 Interfaz
    participant Engine as 🚀 SimulationEngine
    participant Config as ⚙️ ConfigManager
    participant PluginMgr as 🔌 PluginManager
    participant Physics as ⚡ PhysicsIntegrator
    participant Output as 📤 OutputManager
    
    User->>Interface: Iniciar simulación
    Interface->>Engine: initialize(config_file)
    
    Engine->>Config: loadConfig(file)
    Config-->>Engine: Configuración cargada
    
    Engine->>PluginMgr: load_plugins_from_config()
    loop Para cada plugin
        PluginMgr->>PluginMgr: Cargar librería dinámica
        PluginMgr->>PluginMgr: plugin_create_instance()
        PluginMgr->>PluginMgr: plugin_configure(params)
    end
    PluginMgr-->>Engine: Plugins cargados
    
    Engine->>Physics: initialize(integrator_type)
    Physics-->>Engine: Integrador listo
    
    Engine->>Output: initialize(output_config)
    Output-->>Engine: Output configurado
    
    Engine-->>Interface: Sistema inicializado
    Interface-->>User: Listo para simular
    
    User->>Interface: Ejecutar simulación
    Interface->>Engine: run_simulation(max_ticks)
    
    loop Ciclo de simulación
        Engine->>Engine: Incrementar tiempo
        
        Note over Engine,PluginMgr: Fase 1: Plugins Secuenciales
        Engine->>PluginMgr: execute_sequential_plugins()
        loop Para cada plugin tipo 0
            PluginMgr->>PluginMgr: plugin_tick(state)
            Note right of PluginMgr: Modifica estado directamente
        end
        
        Note over Engine,Physics: Fase 2: Plugins Paralelos
        Engine->>PluginMgr: execute_parallel_plugins()
        par Ejecución paralela
            PluginMgr->>PluginMgr: Aerodynamics::plugin_tick()
        and
            PluginMgr->>PluginMgr: Propulsion::plugin_tick()
        and
            PluginMgr->>PluginMgr: Structures::plugin_tick()
        and
            PluginMgr->>PluginMgr: Environment::plugin_tick()
        end
        PluginMgr->>PluginMgr: Acumular fuerzas y torques
        
        Note over Engine,Physics: Fase 3: Integración Física
        Engine->>Physics: integrate(state, forces, dt)
        Physics->>Physics: Aplicar método numérico
        Physics->>Physics: Validar nuevo estado
        Physics-->>Engine: Estado actualizado
        
        Note over Engine,Output: Fase 4: Salida de Datos
        Engine->>Output: write_state(state, tick)
        Output->>Output: Formatear datos (CSV/JSON)
        Output->>Output: Escribir a archivos
        
        Engine->>Engine: Verificar condiciones de parada
        
        alt Simulación completa
            Engine-->>Interface: Simulación terminada
            Interface-->>User: Resultados disponibles
        else Continuar
            Engine->>Engine: Siguiente tick
        end
    end
```

## Diagrama de Flujo Detallado

```mermaid
flowchart TD
    START([🚀 Inicio de Simulación]) --> LOAD_CONFIG{📋 Cargar Configuración}
    
    LOAD_CONFIG -->|Éxito| VALIDATE_CONFIG[✅ Validar Parámetros]
    LOAD_CONFIG -->|Error| ERROR_CONFIG[❌ Error de Configuración]
    ERROR_CONFIG --> END_ERROR([🛑 Fin con Error])
    
    VALIDATE_CONFIG --> LOAD_PLUGINS{🔌 Cargar Plugins}
    
    LOAD_PLUGINS -->|Éxito| INIT_PHYSICS[⚡ Inicializar Física]
    LOAD_PLUGINS -->|Error| ERROR_PLUGINS[❌ Error de Plugins]
    ERROR_PLUGINS --> END_ERROR
    
    INIT_PHYSICS --> LOAD_INITIAL_STATE[📊 Cargar Estado Inicial]
    LOAD_INITIAL_STATE --> SETUP_OUTPUT[📤 Configurar Salida]
    SETUP_OUTPUT --> SIMULATION_READY[✅ Sistema Listo]
    
    SIMULATION_READY --> TICK_START[⏰ Iniciar Tick]
    
    TICK_START --> INCREMENT_TIME[⏱️ Incrementar Tiempo]
    INCREMENT_TIME --> EXEC_SEQUENTIAL[📝 Ejecutar Plugins Secuenciales]
    
    EXEC_SEQUENTIAL --> CHECK_SEQ_ERRORS{❓ Errores Secuenciales}
    CHECK_SEQ_ERRORS -->|Sí| HANDLE_SEQ_ERROR[🔧 Manejar Error]
    CHECK_SEQ_ERRORS -->|No| EXEC_PARALLEL[⚡ Ejecutar Plugins Paralelos]
    
    HANDLE_SEQ_ERROR --> CONTINUE_DECISION{🤔 ¿Continuar?}
    CONTINUE_DECISION -->|Sí| EXEC_PARALLEL
    CONTINUE_DECISION -->|No| END_ERROR
    
    EXEC_PARALLEL --> ACCUMULATE_FORCES[➕ Acumular Fuerzas]
    ACCUMULATE_FORCES --> CHECK_PAR_ERRORS{❓ Errores Paralelos}
    
    CHECK_PAR_ERRORS -->|Sí| HANDLE_PAR_ERROR[🔧 Manejar Error]
    CHECK_PAR_ERRORS -->|No| PHYSICS_INTEGRATION[🧮 Integración Física]
    
    HANDLE_PAR_ERROR --> PHYSICS_INTEGRATION
    
    PHYSICS_INTEGRATION --> VALIDATE_STATE{✅ Validar Estado}
    
    VALIDATE_STATE -->|Válido| UPDATE_STATE[🔄 Actualizar Estado]
    VALIDATE_STATE -->|Inválido| HANDLE_PHYSICS_ERROR[🔧 Error Físico]
    
    HANDLE_PHYSICS_ERROR --> RECOVERY_DECISION{🤔 ¿Recuperar?}
    RECOVERY_DECISION -->|Sí| RESTORE_STATE[↩️ Restaurar Estado]
    RECOVERY_DECISION -->|No| END_ERROR
    RESTORE_STATE --> UPDATE_STATE
    
    UPDATE_STATE --> OUTPUT_DATA[📊 Generar Salida]
    OUTPUT_DATA --> CHECK_STOP_CONDITIONS{🛑 ¿Condiciones de Parada?}
    
    CHECK_STOP_CONDITIONS -->|Continuar| TICK_START
    CHECK_STOP_CONDITIONS -->|Parar| FINALIZE[🏁 Finalizar Simulación]
    
    FINALIZE --> CLEANUP[🧹 Limpiar Recursos]
    CLEANUP --> GENERATE_REPORT[📋 Generar Reporte]
    GENERATE_REPORT --> END_SUCCESS([✅ Fin Exitoso])
    
    %% Estilos
    classDef startEnd fill:#e8f5e8,stroke:#2e7d32,stroke-width:3px
    classDef process fill:#e3f2fd,stroke:#1565c0,stroke-width:2px
    classDef decision fill:#fff3e0,stroke:#ef6c00,stroke-width:2px
    classDef error fill:#ffebee,stroke:#c62828,stroke-width:2px
    classDef success fill:#e8f5e8,stroke:#388e3c,stroke-width:2px
    
    class START,END_SUCCESS,END_ERROR startEnd
    class INCREMENT_TIME,EXEC_SEQUENTIAL,EXEC_PARALLEL,ACCUMULATE_FORCES,PHYSICS_INTEGRATION,UPDATE_STATE,OUTPUT_DATA process
    class LOAD_CONFIG,LOAD_PLUGINS,CHECK_SEQ_ERRORS,CHECK_PAR_ERRORS,VALIDATE_STATE,CHECK_STOP_CONDITIONS decision
    class ERROR_CONFIG,ERROR_PLUGINS,HANDLE_SEQ_ERROR,HANDLE_PAR_ERROR,HANDLE_PHYSICS_ERROR error
    class SIMULATION_READY,FINALIZE success
```

## Fases Detalladas de Ejecución

### 🏁 **Fase 1: Inicialización**

```mermaid
flowchart LR
    subgraph "📋 Configuración"
        A1[Cargar JSON] --> A2[Validar Parámetros]
        A2 --> A3[Aplicar Defaults]
    end
    
    subgraph "🔌 Plugins"
        B1[Escanear Directorio] --> B2[Cargar Librerías]
        B2 --> B3[Crear Instancias]
        B3 --> B4[Configurar Parámetros]
    end
    
    subgraph "⚡ Física"
        C1[Seleccionar Integrador] --> C2[Configurar Tolerancia]
        C2 --> C3[Inicializar Estado]
    end
    
    subgraph "📤 Salida"
        D1[Crear Directorios] --> D2[Configurar Formatos]
        D2 --> D3[Inicializar Archivos]
    end
    
    A3 --> B1
    B4 --> C1
    C3 --> D1
```

### ⚡ **Fase 2: Ciclo de Simulación**

```mermaid
flowchart TD
    TICK_START([⏰ Inicio de Tick]) --> TIME_UPDATE[⏱️ Actualizar Tiempo]
    
    TIME_UPDATE --> SEQ_PHASE[📝 Fase Secuencial]
    
    subgraph "📝 Plugins Secuenciales"
        SEQ1[Plugin Config] --> SEQ2[Plugin Logger]
        SEQ2 --> SEQ3[Plugin Controller]
    end
    
    SEQ_PHASE --> SEQ1
    SEQ3 --> PAR_PHASE[⚡ Fase Paralela]
    
    subgraph "⚡ Plugins Paralelos"
        PAR1[🌪️ Aerodynamics]
        PAR2[🔥 Propulsion]
        PAR3[🏗️ Structures]
        PAR4[🌍 Environment]
    end
    
    PAR_PHASE --> PAR1
    PAR_PHASE --> PAR2
    PAR_PHASE --> PAR3
    PAR_PHASE --> PAR4
    
    PAR1 --> ACCUMULATE[➕ Acumular Fuerzas]
    PAR2 --> ACCUMULATE
    PAR3 --> ACCUMULATE
    PAR4 --> ACCUMULATE
    
    ACCUMULATE --> INTEGRATE[🧮 Integrar Física]
    
    subgraph "🧮 Integración Numérica"
        INT1[Calcular Derivadas] --> INT2[Aplicar Método]
        INT2 --> INT3[Validar Resultado]
    end
    
    INTEGRATE --> INT1
    INT3 --> OUTPUT[📊 Generar Salida]
    
    OUTPUT --> CHECK_STOP{🛑 ¿Parar?}
    CHECK_STOP -->|No| TICK_START
    CHECK_STOP -->|Sí| END_TICK([🏁 Fin])
```

### 📊 **Fase 3: Procesamiento de Plugins**

```mermaid
sequenceDiagram
    participant PluginMgr as PluginManager
    participant AeroPlugin as Aerodynamics
    participant PropPlugin as Propulsion
    participant StructPlugin as Structures
    participant EnvPlugin as Environment
    participant Accumulator as Force Accumulator
    
    Note over PluginMgr: Inicio de fase paralela
    
    par Ejecución Paralela
        PluginMgr->>+AeroPlugin: plugin_tick(state)
        AeroPlugin->>AeroPlugin: Calcular drag/lift
        AeroPlugin-->>-PluginMgr: Force_aero, Torque_aero
    and
        PluginMgr->>+PropPlugin: plugin_tick(state)
        PropPlugin->>PropPlugin: Calcular thrust
        PropPlugin-->>-PluginMgr: Force_prop, Torque_prop
    and
        PluginMgr->>+StructPlugin: plugin_tick(state)
        StructPlugin->>StructPlugin: Actualizar masa/CG
        StructPlugin-->>-PluginMgr: Force_struct, Torque_struct
    and
        PluginMgr->>+EnvPlugin: plugin_tick(state)
        EnvPlugin->>EnvPlugin: Efectos ambientales
        EnvPlugin-->>-PluginMgr: Force_env, Torque_env
    end
    
    Note over PluginMgr: Sincronización
    
    PluginMgr->>Accumulator: Sumar todas las fuerzas
    Accumulator->>Accumulator: F_total = F_aero + F_prop + F_struct + F_env
    Accumulator->>Accumulator: T_total = T_aero + T_prop + T_struct + T_env
    Accumulator-->>PluginMgr: Fuerzas y torques totales
```

## Manejo de Errores y Recuperación

### 🔧 **Estrategias de Manejo de Errores**

```mermaid
flowchart TD
    ERROR_DETECTED[❌ Error Detectado] --> CLASSIFY_ERROR{🔍 Clasificar Error}
    
    CLASSIFY_ERROR -->|Configuración| CONFIG_ERROR[⚙️ Error de Configuración]
    CLASSIFY_ERROR -->|Plugin| PLUGIN_ERROR[🔌 Error de Plugin]
    CLASSIFY_ERROR -->|Física| PHYSICS_ERROR[⚡ Error de Física]
    CLASSIFY_ERROR -->|I/O| IO_ERROR[📁 Error de E/S]
    
    CONFIG_ERROR --> CONFIG_RECOVERY{🔧 ¿Recuperable?}
    CONFIG_RECOVERY -->|Sí| APPLY_DEFAULTS[📋 Aplicar Defaults]
    CONFIG_RECOVERY -->|No| ABORT_SIMULATION[🛑 Abortar]
    
    PLUGIN_ERROR --> PLUGIN_RECOVERY{🔧 ¿Plugin Crítico?}
    PLUGIN_RECOVERY -->|No| DISABLE_PLUGIN[🚫 Deshabilitar Plugin]
    PLUGIN_RECOVERY -->|Sí| RELOAD_PLUGIN[🔄 Recargar Plugin]
    
    PHYSICS_ERROR --> PHYSICS_RECOVERY{🔧 ¿Estado Válido?}
    PHYSICS_RECOVERY -->|Sí| RESTORE_STATE[↩️ Restaurar Estado]
    PHYSICS_RECOVERY -->|No| REDUCE_TIMESTEP[⏱️ Reducir Timestep]
    
    IO_ERROR --> IO_RECOVERY{🔧 ¿Espacio Disponible?}
    IO_RECOVERY -->|Sí| RETRY_OPERATION[🔄 Reintentar]
    IO_RECOVERY -->|No| CHANGE_OUTPUT[📁 Cambiar Salida]
    
    APPLY_DEFAULTS --> CONTINUE_SIMULATION[✅ Continuar]
    DISABLE_PLUGIN --> CONTINUE_SIMULATION
    RELOAD_PLUGIN --> CONTINUE_SIMULATION
    RESTORE_STATE --> CONTINUE_SIMULATION
    REDUCE_TIMESTEP --> CONTINUE_SIMULATION
    RETRY_OPERATION --> CONTINUE_SIMULATION
    CHANGE_OUTPUT --> CONTINUE_SIMULATION
    
    ABORT_SIMULATION --> LOG_ERROR[📝 Log Error]
    LOG_ERROR --> CLEANUP_RESOURCES[🧹 Limpiar]
    CLEANUP_RESOURCES --> END_WITH_ERROR([❌ Fin con Error])
    
    CONTINUE_SIMULATION --> LOG_WARNING[⚠️ Log Warning]
    LOG_WARNING --> RESUME_EXECUTION[▶️ Reanudar]
```

### 📊 **Métricas de Rendimiento por Fase**

| Fase | Tiempo Objetivo | Tiempo Típico | Porcentaje |
|------|----------------|---------------|------------|
| Inicialización | < 100ms | 80ms | - |
| Plugins Secuenciales | < 0.1ms | 0.05ms | 5% |
| Plugins Paralelos | < 0.5ms | 0.3ms | 30% |
| Integración Física | < 0.2ms | 0.15ms | 15% |
| Validación Estado | < 0.1ms | 0.05ms | 5% |
| Salida de Datos | < 0.4ms | 0.25ms | 25% |
| Overhead Sistema | < 0.2ms | 0.15ms | 15% |
| **Total por Tick** | **< 1.5ms** | **< 1.0ms** | **100%** |

### 🎯 **Condiciones de Parada**

```mermaid
flowchart LR
    CHECK_CONDITIONS[🔍 Verificar Condiciones] --> TIME_LIMIT{⏰ Límite de Tiempo}
    CHECK_CONDITIONS --> TICK_LIMIT{🔢 Límite de Ticks}
    CHECK_CONDITIONS --> ALTITUDE_LIMIT{🏔️ Límite de Altitud}
    CHECK_CONDITIONS --> VELOCITY_LIMIT{🚀 Límite de Velocidad}
    CHECK_CONDITIONS --> USER_STOP{👤 Parada Manual}
    CHECK_CONDITIONS --> ERROR_LIMIT{❌ Límite de Errores}
    
    TIME_LIMIT -->|Alcanzado| STOP_SIMULATION[🛑 Parar Simulación]
    TICK_LIMIT -->|Alcanzado| STOP_SIMULATION
    ALTITUDE_LIMIT -->|Alcanzado| STOP_SIMULATION
    VELOCITY_LIMIT -->|Alcanzado| STOP_SIMULATION
    USER_STOP -->|Solicitado| STOP_SIMULATION
    ERROR_LIMIT -->|Excedido| STOP_SIMULATION
    
    TIME_LIMIT -->|No| CONTINUE[✅ Continuar]
    TICK_LIMIT -->|No| CONTINUE
    ALTITUDE_LIMIT -->|No| CONTINUE
    VELOCITY_LIMIT -->|No| CONTINUE
    USER_STOP -->|No| CONTINUE
    ERROR_LIMIT -->|No| CONTINUE
    
    STOP_SIMULATION --> FINALIZE[🏁 Finalizar]
    CONTINUE --> NEXT_TICK[➡️ Siguiente Tick]
```

## Optimizaciones de Flujo

### ⚡ **Paralelización**

- **Plugins Paralelos**: Ejecución simultánea en threads separados
- **I/O Asíncrono**: Escritura de datos en background
- **Pipeline**: Solapamiento de fases cuando sea posible
- **Cache**: Reutilización de cálculos costosos

### 🎯 **Predicción y Adaptación**

- **Timestep Adaptativo**: Ajuste automático según estabilidad
- **Plugin Scheduling**: Priorización basada en importancia
- **Memory Pooling**: Reutilización de objetos frecuentes
- **Lazy Loading**: Carga bajo demanda de recursos

Este flujo de simulación garantiza ejecución eficiente, robusta y escalable del sistema MoLab, manteniendo alta precisión física y excelente rendimiento computacional.
