# MoLab — Diagrama de Arquitectura (Lucidchart)

## Importar en Lucidchart

**Insert → Import → Mermaid** → pegar código → Insert

---

## Diagrama 1 — Flujo de Ejecucion

```mermaid
flowchart TD
    START(["Inicio"])
    CONFIG["Cargar configuracion y Logger"]
    MODE{{"CLI o IPC?"}}
    INIT["Inicializar motor\nCargar estado inicial y plugins"]
    TICK[["Bucle: run_tick()"]]
    P1["Plugins secuenciales"]
    P2["Plugins paralelos"]
    P3["Integracion fisica"]
    P4["Registrar resultados"]
    SHUT["shutdown()"]
    END_NODE(["Fin"])

    START --> CONFIG --> MODE
    MODE -->|"CLI"| INIT
    MODE -->|"IPC stdio"| INIT
    INIT --> TICK
    TICK --> P1 --> P2 --> P3 --> P4
    P4 -->|"siguiente tick"| TICK
    TICK -->|"completado"| SHUT --> END_NODE
```

---

## Diagrama 2 — Componentes

```mermaid
flowchart LR
    subgraph IN["Entrada"]
        A["CLI / IPC"]
    end
    subgraph CORE["Nucleo"]
        B["ConfigManager"]
        C["SimulationEngine"]
    end
    subgraph CALC["Calculo"]
        D["PluginManager"]
        E["PhysicsIntegrator"]
    end
    subgraph OUT["Salida"]
        F["OutputManager"]
    end

    IN --> CORE --> CALC --> OUT
```

---

## Diagrama 3 — Flujo de Ejecucion (detallado)

```mermaid
flowchart TD
    START(["Inicio"])
    MAIN["main.cpp\nEjecutar simulador"]
    CONFIG["ConfigManager::loadConfig\nCargar configuracion JSON"]
    LOGGER["Logger bootstrap\nNivel, consola, archivo"]

    MODE{{"Modo de ejecucion?"}}
    IPC["IpcSession\nComandos JSON stdin/stdout"]
    CLI["Modo CLI\n--config --ticks"]

    INIT["SimulationEngine::initialize_from_loaded_config"]
    STATE_LOAD["InitialStateLoader\nJSON → FlatBuffers state_buffer"]
    PLUGINS_LOAD["PluginManager::load_plugins_from_config\nCargar plugins habilitados via dlopen"]

    TICK_LOOP[["Bucle: run_tick()"]]

    PHASE1["Fase 1\nPlugins secuenciales\nModifican state_buffer"]
    PHASE2["Fase 2\nPlugins paralelos en threads\nCalculan fuerzas y torques"]
    PHASE3["Fase 3\nPhysicsIntegrator\nIntegracion de ecuaciones de movimiento\nEuler / RK4 / Verlet"]
    PHASE4["Fase 4\nTimeManager + OutputManager\nActualizar tiempo y registrar resultados\nCSV / JSON / binario"]

    SHUTDOWN["SimulationEngine::shutdown\nLiberar plugins y recursos"]
    END_NODE(["Fin"])

    START --> MAIN
    MAIN --> CONFIG
    CONFIG --> LOGGER
    LOGGER --> MODE

    MODE -->|"--ipc stdio"| IPC
    MODE -->|"CLI"| CLI

    IPC --> INIT
    CLI --> INIT

    INIT --> STATE_LOAD
    STATE_LOAD --> PLUGINS_LOAD
    PLUGINS_LOAD --> TICK_LOOP

    TICK_LOOP --> PHASE1
    PHASE1 --> PHASE2
    PHASE2 --> PHASE3
    PHASE3 --> PHASE4
    PHASE4 -->|"siguiente tick"| TICK_LOOP

    TICK_LOOP -->|"ticks completados"| SHUTDOWN
    SHUTDOWN --> END_NODE
```
