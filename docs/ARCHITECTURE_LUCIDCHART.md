# Apogeo Architecture Diagrams for Lucidchart

## Importing a Diagram

Each Mermaid block below is source code for a diagram. To import one into
Lucidchart, copy the complete block without the surrounding Markdown fences,
then select **Insert**, **Import**, and **Mermaid** in Lucidchart. Paste the
code in the import dialog and select **Insert**.

## Diagram 1: Execution Flow

```mermaid
flowchart TD
    Start([Start])
    Config[Load configuration and logger]
    Mode{CLI or IPC?}
    Initialize[Initialize engine, state, and plugins]
    Tick[Run simulation tick]
    Sequential[Run sequential plugins]
    Parallel[Run parallel plugins]
    Physics[Integrate physics]
    Output[Record results]
    Shutdown[Release resources]
    End([End])

    Start --> Config --> Mode --> Initialize --> Tick
    Tick --> Sequential --> Parallel --> Physics --> Output --> Tick
    Tick --> Shutdown --> End
```

## Diagram 2: Components

```mermaid
flowchart LR
    Input[CLI and IPC]
    Core[ConfigManager and SimulationEngine]
    Calculation[PluginManager and PhysicsIntegrator]
    Results[OutputManager]

    Input --> Core --> Calculation --> Results
```

## Diagram 3: Detailed Execution Flow

```mermaid
flowchart TD
    Start([Start])
    Main[main.cpp]
    Config[ConfigManager loads JSON configuration]
    Logger[Initialize logger]
    Mode{Execution mode}
    Cli[CLI arguments]
    Ipc[IpcSession over standard input and output]
    Engine[SimulationEngine initialization]
    State[InitialStateLoader builds the FlatBuffers state]
    Plugins[PluginManager loads enabled plugins]
    Tick[Simulation tick]
    Sequential[Sequential plugins update state]
    Parallel[Parallel plugins calculate force and torque]
    Physics[PhysicsIntegrator updates motion]
    Output[TimeManager and OutputManager]
    Shutdown[SimulationEngine shutdown]
    End([End])

    Start --> Main --> Config --> Logger --> Mode
    Mode --> Cli --> Engine
    Mode --> Ipc --> Engine
    Engine --> State --> Plugins --> Tick
    Tick --> Sequential --> Parallel --> Physics --> Output --> Tick
    Tick --> Shutdown --> End
```
