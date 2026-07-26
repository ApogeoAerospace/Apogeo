# MoLab — Diagrama de Arquitectura

## Diagrama General del Sistema

```mermaid
graph TB
    subgraph ENTRYPOINT["🚀 Punto de Entrada"]
        MAIN["main.cpp\n─────────────\n• Parse CLI args\n• --config, --ticks\n• --log-level\n• --ipc stdio"]
    end

    subgraph CONFIG_LAYER["⚙️ Capa de Configuración"]
        CFG["ConfigManager\n─────────────\n• Singleton\n• loadConfig()\n• SimulationConfig\n• PluginConfig[]\n• initial_state_file\n• output_directory"]
        CFG_FILE[["default_config.json\n(data/defaults/)"]]
        CFG_FILE -->|"lee"| CFG
    end

    subgraph LOGGING["📋 Logging"]
        LOG["Logger\n─────────────\n• Singleton\n• Console / File\n• Buffer 256 entries\n• setStructuredSink()\n• Niveles: DEBUG→CRIT"]
        LOG_FILE[["simulation.log\n(archivo)"]]
        LOG -->|"escribe"| LOG_FILE
    end

    subgraph IPC_LAYER["🔌 IPC stdio (opcional)"]
        IPC["IpcSession\n─────────────\n• runIpcStdioSession()\n• Lee JSON-lines stdin\n• Emite JSON-lines stdout"]
        CMD["CommandEventProtocol\n─────────────\n• parseCommandJsonLine()\n• buildAckJson()\n• buildErrorJson()\n• buildEventJson()"]
        STDIN(["stdin\n(JSON commands)"])
        STDOUT(["stdout\n(JSON events/acks)"])
        STDIN -->|"lee"| IPC
        IPC -->|"escribe"| STDOUT
        IPC <-->|"usa"| CMD
        LOG -->|"structured sink"| IPC
    end

    subgraph ENGINE["🏗️ SimulationEngine"]
        SE["SimulationEngine\n─────────────\n• initialize_from_loaded_config()\n• run_tick()\n• run_simulation()\n• shutdown()\n• getStatus()"]
        STATE["State Buffer\n(std::vector uint8_t)\n─────────────\n• FlatBuffers binario\n• GeneralState\n• mutex protegido"]
    end

    subgraph TIME["⏱️ TimeManager"]
        TM["TimeManager\n─────────────\n• Singleton\n• Tiempo relativo sim\n• Tiempo UTC absoluto\n• updateSimulationTime()\n• atomic + mutex"]
    end

    subgraph INIT_STATE["📂 Carga de Estado Inicial"]
        ISL["InitialStateLoader\n─────────────\n• loadFromFile()\n• JSON → FlatBuffers\n• GeneralState"]
        STATE_FILE[["default_state.json\n(data/defaults/)"]]
        STATE_FILE -->|"lee"| ISL
        ISL -->|"serializa"| STATE
    end

    subgraph PLUGIN_SYSTEM["🔧 Sistema de Plugins"]
        PM["PluginManager\n─────────────\n• load_plugins_from_config()\n• run_simulation_cycle()\n• dlopen / LoadLibrary\n• PluginHostServices bridge"]

        subgraph SCHEDULER["🧵 PluginTaskScheduler"]
            PTS["PluginTaskScheduler\n─────────────\n• Thread pool\n• Plugins paralelos\n• Acumulación F/T"]
        end

        subgraph SEQ_PLUGINS["Plugins Secuenciales (type=0)"]
            PLUG_SEQ["execute_sequential_plugins()\n─────────────\n• Modifican state buffer\n• Ejecución ordenada"]
        end

        subgraph PAR_PLUGINS["Plugins Paralelos (type=1)"]
            PLUG_PAR["execute_parallel_plugins()\n─────────────\n• Calculan Force/Torque\n• Ejecución concurrente\n• Acumulan F+T"]
        end

        PM --> SEQ_PLUGINS
        PM --> SCHEDULER
        SCHEDULER --> PAR_PLUGINS
    end

    subgraph PLUGIN_IMPLS["📦 Plugins Dinámicos (.dylib/.so/.dll)"]
        PLUG_STRUCT["structures\n─────────────\n• PARALLEL (type=1)\n• masa/inercia JSON\n• límites CSV\n• checkStructuralIntegrity()\n• F=0, T=0 (pendiente)"]
        PLUG_PROP["propulsion\n(en desarrollo)"]
        PLUG_EXAMPLE["example_plugin\n(referencia API)"]
    end

    subgraph PLUGIN_API_CONTRACT["📜 Contrato Plugin API (C)"]
        API["plugin_api.h\n─────────────\n• plugin_create_instance()\n• plugin_tick(handle, data*)\n• plugin_destroy_instance()\n• plugin_configure() [opt]\n• plugin_set_host_services() [opt]\n• PluginTickData\n• PluginHostServices"]
    end

    subgraph PHYSICS["⚛️ Integrador Físico"]
        PI["PhysicsIntegrator\n─────────────\n• integrate(state, F, T, dt)\n• OdeState[13]:\n  pos(3)+vel(3)+quat(4)+ω(3)\n• Boost.Odeint\n• Eigen (quaternion, tensor)"]
        EULER["Euler"]
        RK4["Runge-Kutta 4\n(default)"]
        VERLET["Verlet"]
        PI --> EULER
        PI --> RK4
        PI --> VERLET
    end

    subgraph OUTPUT["💾 OutputManager"]
        OM["OutputManager\n─────────────\n• Singleton\n• recordState()\n• Writer thread async\n• setRealtimeTelemetryCallback()\n• SimulationDataPoint"]
        OUT_CSV[["output/*.csv"]]
        OUT_JSON[["output/*.json"]]
        OUT_BIN[["output/*.bin"]]
        OM -->|"escribe"| OUT_CSV
        OM -->|"escribe"| OUT_JSON
        OM -->|"escribe"| OUT_BIN
    end

    subgraph DATA_FILES["📁 Archivos de Datos"]
        MASS_FILE[["mass_properties.json\n(data/mass/)"]]
        LIMITS_FILE[["structural_limits.csv\n(data/limits/)"]]
        FB_SCHEMA[["state_vector.fbs\n(src/schemas/)\n→ state_vector_generated.h"]]
    end

    subgraph TESTS["🧪 Tests (Google Test)"]
        T1["test_config_manager"]
        T2["test_physics_integrator"]
        T3["test_time_manager"]
        T4["test_logger"]
        T5["test_output_manager"]
        T6["test_simulation_engine"]
        T7["test_initial_state_loader"]
        T8["test_command_event_protocol"]
        T9["test_ipc_session"]
    end

    %% === FLUJO PRINCIPAL ===
    MAIN -->|"loadConfig()"| CFG
    MAIN -->|"bootstrap"| LOG
    MAIN -->|"crea"| SE
    MAIN -->|"modo normal"| SE
    MAIN -->|"--ipc stdio"| IPC
    IPC -->|"controla"| SE

    CFG -->|"SimulationConfig\nPluginConfig[]"| SE
    CFG -->|"initial_state_file"| ISL

    SE -->|"usa"| PM
    SE -->|"usa"| TM
    SE -->|"usa"| OM
    SE <-->|"lee/escribe"| STATE

    PM -->|"run_simulation_cycle()"| PLUG_SEQ
    PM -->|"run_simulation_cycle()"| PLUG_PAR
    PM -->|"apply_physics_integration()"| PI
    PI -->|"estado integrado"| STATE
    PLUG_SEQ <-->|"PluginTickData\n(state buffer)"| STATE

    PM <-->|"API C"| API
    API <-->|"implementan"| PLUG_STRUCT
    API <-->|"implementan"| PLUG_PROP
    API <-->|"implementan"| PLUG_EXAMPLE

    PLUG_STRUCT -->|"lee"| MASS_FILE
    PLUG_STRUCT -->|"lee"| LIMITS_FILE

    SE -->|"recordState()"| OM
    OM -->|"telemetry cb"| IPC

    LOG -->|"emite eventos"| IPC

    FB_SCHEMA -->|"genera"| ISL
    FB_SCHEMA -->|"genera"| PI
    FB_SCHEMA -->|"genera"| OM
```

---

## Diagrama del Ciclo de Tick

```mermaid
sequenceDiagram
    participant Main
    participant SE as SimulationEngine
    participant PM as PluginManager
    participant SEQ as Sequential Plugins
    participant PAR as Parallel Plugins (threads)
    participant PI as PhysicsIntegrator
    participant TM as TimeManager
    participant OM as OutputManager

    Main->>SE: run_tick()
    activate SE

    SE->>PM: run_simulation_cycle(state_buffer, dt)
    activate PM

    PM->>SEQ: execute_sequential_plugins()
    loop Cada plugin type=0
        SEQ->>SEQ: plugin_tick(handle, TickData)
        note right of SEQ: Modifica state_buffer directamente
    end

    PM->>PAR: execute_parallel_plugins() [threads]
    par Plugin A
        PAR->>PAR: plugin_tick() → force_A, torque_A
    and Plugin B
        PAR->>PAR: plugin_tick() → force_B, torque_B
    end
    PAR-->>PM: accumulated_force, accumulated_torque

    PM->>PI: apply_physics_integration(state_buffer, F_total, T_total, dt)
    PI->>PI: FlatBuffers → PhysicsState
    PI->>PI: integrate(OdeState[13], dt)
    PI->>PI: PhysicsState → FlatBuffers
    PI-->>PM: state_buffer actualizado

    deactivate PM

    SE->>TM: updateSimulationTime(dt)
    SE->>OM: recordState(state_buffer, sim_time, utc_time, tick)
    note right of OM: Async writer thread → CSV/JSON/bin
    OM-->>SE: (callback telemetría si IPC activo)

    deactivate SE
    SE-->>Main: tick completado
```

---

## Diagrama del Modo IPC stdio

```mermaid
sequenceDiagram
    participant Client as Cliente Externo
    participant IPC as IpcSession
    participant LOG as Logger
    participant SE as SimulationEngine
    participant OM as OutputManager

    Client->>IPC: stdin: {"type":"command","id":"1","name":"initialize"}
    IPC->>LOG: Replay buffer (últimas 256 entradas como eventos log)
    LOG-->>Client: stdout: {"type":"event","event":"log",...} ×N

    IPC->>SE: engine.initialize_from_loaded_config()
    SE-->>IPC: true
    IPC-->>Client: stdout: {"type":"ack","ok":true,"request_id":"1"}

    Client->>IPC: stdin: {"type":"command","id":"2","name":"run_ticks","payload":{"count":10}}
    IPC-->>Client: stdout: {"type":"event","event":"simulation_started",...}

    loop Cada tick (×10)
        IPC->>SE: engine.run_tick()
        SE->>OM: recordState()
        OM->>IPC: realtime telemetry callback
        IPC-->>Client: stdout: {"type":"event","event":"state_sample",...}
        IPC-->>Client: stdout: {"type":"event","event":"tick_completed",...} [throttled]
    end

    IPC-->>Client: stdout: {"type":"event","event":"simulation_finished",...}
    IPC-->>Client: stdout: {"type":"ack","ok":true,"request_id":"2"}

    Client->>IPC: stdin: {"type":"command","id":"3","name":"shutdown"}
    IPC->>SE: engine.shutdown()
    IPC-->>Client: stdout: {"type":"ack","ok":true,"request_id":"3","data":{"shutdown":true}}
```

---

## Diagrama de Capas del Sistema

```mermaid
graph LR
    subgraph L1["Capa de Entrada / Control"]
        MAIN_L["main.cpp\nCLI / IPC stdio"]
    end

    subgraph L2["Capa de Configuración y Bootstrap"]
        CFG_L["ConfigManager\n(Singleton)"]
        LOG_L["Logger\n(Singleton)"]
    end

    subgraph L3["Capa de Orquestación"]
        SE_L["SimulationEngine"]
        IPC_L["IpcSession"]
    end

    subgraph L4["Capa de Managers de Tiempo y Estado"]
        TM_L["TimeManager\n(Singleton)"]
        ISL_L["InitialStateLoader"]
        STATE_L["FlatBuffers\nState Buffer"]
    end

    subgraph L5["Capa de Plugins y Física"]
        PM_L["PluginManager\n+ PluginTaskScheduler"]
        PI_L["PhysicsIntegrator\n(Euler / RK4 / Verlet)"]
    end

    subgraph L6["Capa de Plugins Externos"]
        API_L["plugin_api.h\n(Contrato C)"]
        PLUG_L["structures.dylib\npropulsion.dylib\n..."]
    end

    subgraph L7["Capa de Salida"]
        OM_L["OutputManager\n(Singleton)\nAsync Writer Thread"]
        FILES_L["CSV / JSON / BIN"]
    end

    L1 --> L2
    L1 --> L3
    L2 --> L3
    L3 --> L4
    L3 --> L5
    L3 --> L7
    L5 --> L6
    L6 <-->|"C ABI"| API_L
    L4 --> L5
    L7 --> FILES_L
```

---

## Dependencias Externas

```mermaid
graph TD
    MOLAB["MoLab Core"]

    subgraph BUILD["Build Time"]
        CMAKE["CMake ≥ 3.20\n+ Ninja"]
        VCPKG["vcpkg\n(gestor dependencias)"]
        FLATC["flatc\n(compilador FlatBuffers)"]
    end

    subgraph RUNTIME_DEPS["Dependencias Runtime"]
        FB["flatbuffers\n(serialización estado)"]
        JSON["nlohmann/json\n(config + IPC)"]
        EIGEN["Eigen 3.4\n(álgebra lineal, quaterniones)"]
        BOOST["Boost.Odeint\n(integración ODE)"]
    end

    subgraph TEST_DEPS["Dependencias de Tests"]
        GTEST["Google Test\n(GTest + CTest)"]
    end

    MOLAB -->|"usa"| FB
    MOLAB -->|"usa"| JSON
    MOLAB -->|"usa"| EIGEN
    MOLAB -->|"usa"| BOOST
    MOLAB -->|"tests"| GTEST
    MOLAB -->|"build"| CMAKE
    CMAKE -->|"deps"| VCPKG
    CMAKE -->|"genera headers"| FLATC
    VCPKG -->|"instala"| FB
    VCPKG -->|"instala"| JSON
    VCPKG -->|"instala"| GTEST
    VCPKG -->|"instala"| BOOST
```
