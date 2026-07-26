# MoLab — Documento Resumen del Proyecto

## 1. Descripción General

**MoLab** (también referido internamente como *Apogeo Aerospace Simulator*) es un **motor de simulación aeroespacial modular** escrito en **C++17**. Su propósito es simular la dinámica de vuelo de vehículos aeroespaciales (cohetes, satélites, etc.) integrando física de cuerpo rígido con un pipeline extensible de plugins de cálculo.

El proyecto se distribuye bajo la licencia **AGPL-3.0** y está diseñado para ejecución en línea de comandos (CLI) con soporte opcional de IPC estructurado sobre `stdin/stdout`.

---

## 2. Objetivos del Sistema

- Simular la evolución física de un vehículo aeroespacial tick a tick (posición, velocidad, orientación, velocidad angular, fuerzas y torques).
- Soportar múltiples métodos de integración numérica (Euler, Runge-Kutta 4, Verlet).
- Proveer un mecanismo de extensión por **plugins dinámicos** (.so/.dylib/.dll) con una API en C puro.
- Permitir control externo del motor por medio de un protocolo IPC JSON-lines (`--ipc stdio`).
- Registrar y exportar resultados de simulación en múltiples formatos (CSV, JSON, binario).

---

## 3. Stack Tecnológico

| Componente | Tecnología |
|---|---|
| Lenguaje | C++17 |
| Sistema de build | CMake ≥ 3.20 + Ninja |
| Gestión de dependencias | vcpkg |
| Serialización de estado | FlatBuffers (schema `state_vector.fbs`) |
| Parsing de configuración | nlohmann/json |
| Álgebra lineal | Eigen 3.4 (header-only, via FetchContent) |
| Integración ODE | Boost.Odeint |
| Framework de tests | Google Test (GTest) + CTest |

---

## 4. Estructura del Repositorio

```
MoLab/
├── src/
│   ├── main.cpp              # Punto de entrada: CLI + bootstrap
│   ├── core/                 # Motores y managers del núcleo
│   ├── api/                  # Contrato C de plugins (plugin_api.h)
│   ├── ipc/                  # Sesión IPC stdio (IpcSession)
│   └── schemas/              # Schema FlatBuffers (state_vector.fbs)
├── plugins/
│   ├── structures/           # Plugin activo: propiedades de masa y límites estructurales
│   ├── propulsion/           # Plugin en desarrollo
│   ├── example_plugin/       # Plantilla de referencia
│   └── deprecated/           # Plugins legados (no en build activo)
├── tests/                    # Suite de pruebas unitarias (Google Test)
├── data/
│   ├── defaults/             # default_config.json, default_state.json
│   ├── mass/                 # mass_properties.json
│   └── limits/               # structural_limits.csv
├── docs/                     # Documentación activa y generada
├── scripts/                  # Scripts de CI y dependencias (bash + PowerShell)
└── tools/                    # Tooling auxiliar (web server legacy)
```

---

## 5. Componentes del Núcleo (`src/core/`)

### 5.1 `SimulationEngine`
Orquestador principal del ciclo de simulación. Es la única clase que combina estado, tiempo, plugins y salida. Posee:
- Inicialización desde `ConfigManager` (`initialize_from_loaded_config()`).
- Ejecución de ticks individuales (`run_tick()`) o ejecución completa (`run_simulation()`).
- Estado interno protegido por `std::mutex` + `std::atomic`.
- Acceso de solo lectura al estado en tiempo real via `getStatus()`.

### 5.2 `ConfigManager` (Singleton)
Carga, valida y expone la configuración desde `default_config.json`. Gestiona:
- `SimulationConfig`: duración, iteraciones, logging, IPC.
- `PluginConfig[]`: nombre, tipo, ruta de librería, parámetros JSON.
- Rutas: `initial_state_file`, `output_directory`.

### 5.3 `PluginManager`
Gestiona el ciclo de vida de los plugins dinámicos (carga por `dlopen`/`LoadLibrary`, ejecución, descarga). Coordina:
- **Plugins secuenciales** (`type=0`): modifican directamente el buffer de estado.
- **Plugins paralelos** (`type=1`): calculan fuerzas/torques concurrentemente via `PluginTaskScheduler`.
- Inyección opcional de `PluginHostServices` (logging bridge hacia el host).
- Integración física tras cada ciclo de plugins (`apply_physics_integration()`).

### 5.4 `PhysicsIntegrator`
Integrador numérico de ecuaciones diferenciales del movimiento de cuerpo rígido (6 DOF). Opera sobre:
- Estado plano ODE de 13 componentes: `pos(3) + vel(3) + quat(4) + omega(3)`.
- Métodos soportados: `EULER`, `RUNGE_KUTTA_4` (default), `VERLET`.
- Usa **Boost.Odeint** para integración y **Eigen** para álgebra de quaterniones y tensor de inercia.
- Convierte de/hacia FlatBuffers (`fromFlatBuffer()`).

### 5.5 `OutputManager` (Singleton)
Maneja la persistencia asíncrona de resultados mediante un **writer thread** dedicado. Soporta:
- Formatos: CSV, JSON, binario.
- Telemetría en tiempo real via callback configurable (`setRealtimeTelemetryCallback()`).
- Datos: cinemática, masa, aerodinámica, ambiente y gravedad.

### 5.6 `TimeManager` (Singleton)
Gestiona tiempo de simulación relativo y tiempo UTC absoluto. Proporciona conversiones y sincronización thread-safe.

### 5.7 `InitialStateLoader`
Carga el estado inicial de la simulación desde un archivo JSON y lo serializa en formato FlatBuffers como `GeneralState`.

### 5.8 `Logger` (Singleton)
Sistema de logging centralizado con:
- Salida a consola y/o archivo.
- Buffer circular de 256 entradas para replay en modo IPC.
- Sink estructurado configurable (`setStructuredSink()`) usado por `IpcSession`.
- Niveles: `DEBUG`, `INFO`, `WARNING`, `ERROR`, `CRITICAL`.

### 5.9 `IpcSession` + `CommandEventProtocol`
Maneja el modo `--ipc stdio`. Implementa:
- Lectura de comandos JSON-lines desde `stdin`.
- Emisión de respuestas/eventos JSON-lines a `stdout`.
- Comandos: `get_status`, `initialize`, `run_ticks`, `run_full`, `shutdown`.
- Eventos: `log`, `error`, `simulation_started`, `tick_completed`, `simulation_finished`, `state_sample`.
- Replay del buffer de logs al inicio de sesión.

---

## 6. Plugin Activo: `structures`

El plugin `structures` es actualmente el único plugin integrado en el build. Implementa:
- Carga de propiedades de masa/inercia desde `data/mass/mass_properties.json`.
- Carga de límites estructurales desde `data/limits/structural_limits.csv`.
- Verificación de integridad estructural en runtime (`checkStructuralIntegrity()`).
- **Salida actual**: fuerza y torque = cero (física estructural pendiente de implementación).
- Integración con el logger del host via `plugin_set_host_services()`.

---

## 7. Contrato de Plugin (`plugin_api.h`)

API en C puro (sin name mangling) que toda librería dinámica debe exportar:

| Función | Obligatoria | Descripción |
|---|---|---|
| `plugin_create_instance()` | Sí | Crea instancia y retorna handle opaco |
| `plugin_tick(handle, data*)` | Sí | Ejecuta un tick de simulación |
| `plugin_destroy_instance(handle)` | Sí | Libera la instancia |
| `plugin_configure(handle, json*)` | No | Configura con parámetros JSON |
| `plugin_set_host_services(services*)` | No | Recibe servicios del host (logger) |

---

## 8. Flujo de Ejecución

### Modo CLI
```
main.cpp
  │
  ├── Parse CLI args (--config, --ticks, --log-level)
  ├── ConfigManager::loadConfig(file)
  ├── Logger bootstrap (nivel, archivo, consola)
  ├── SimulationEngine::initialize_from_loaded_config()
  │     ├── PluginManager::load_plugins_from_config()
  │     │     └── dlopen + plugin_create_instance + plugin_configure
  │     └── InitialStateLoader → FlatBuffers state buffer
  └── Loop: engine.run_tick() × N
        ├── PluginManager::run_simulation_cycle()
        │     ├── execute_sequential_plugins()  [type=0, mutate state]
        │     ├── execute_parallel_plugins()    [type=1, calc force/torque]
        │     └── apply_physics_integration()   [PhysicsIntegrator]
        ├── TimeManager::updateSimulationTime()
        └── OutputManager::recordState()        [async writer thread]
```

### Modo IPC stdio
```
main.cpp --ipc stdio
  │
  ├── Logger: disable console, enable structured sink → IpcSession
  └── IpcSession::run()
        ├── Replay buffer de logs como eventos
        └── Loop: read JSON line → dispatch command
              ├── get_status → ack con estado actual
              ├── initialize → engine.initialize_from_loaded_config()
              ├── run_ticks  → N × engine.run_tick() + eventos
              ├── run_full   → engine.run_simulation() + eventos
              └── shutdown   → engine.shutdown() + ack
```

---

## 9. Configuración

La configuración se carga desde `data/defaults/default_config.json` y estructura:

| Sección | Propósito |
|---|---|
| `simulation` | Duración, max iteraciones, log level |
| `plugins[]` | Plugin name, type, library_path, enabled, parameters |
| `initial_state_file` | Ruta al JSON de estado inicial |
| `output_directory` | Directorio de salida de resultados |
| `logging` | Console/file output, rotación |
| `ipc` | Intervalos de eventos y telemetría |
| `performance` | Métricas, profiling, thread pool |
| `validation` | Límites de magnitud, checks NaN |
| `vehicle_models` | Aero, geometría, propulsión, masa, límites (reservado) |
| `mission_environment` | Planeta, atmósfera, secuencia de vuelo (reservado) |

---

## 10. Pruebas

Suite de tests unitarios con Google Test (9 archivos):

| Test | Cobertura |
|---|---|
| `test_config_manager` | Carga, validación y defaults de ConfigManager |
| `test_physics_integrator` | Métodos de integración (Euler, RK4, Verlet), 6-DOF |
| `test_time_manager` | Tiempo relativo, UTC, conversiones |
| `test_logger` | Niveles, buffer circular, sink estructurado |
| `test_output_manager` | CSV/JSON, async writer, telemetría |
| `test_simulation_engine` | Ciclo completo, inicialización, shutdown |
| `test_initial_state_loader` | Carga de estado desde JSON, FlatBuffers |
| `test_command_event_protocol` | Parsing y construcción de mensajes IPC |
| `test_ipc_session` | Flujo de sesión IPC stdio |

---

## 11. Gaps Conocidos

1. `output_directory` del config no se aplica al motor (path hardcoded).
2. `simulation.enable_logging` no actúa como switch global.
3. Rotación de logs (`log_rotation`, `max_file_size_mb`, `max_files`) no implementada.
4. Secciones `performance.*`, `validation.*`, `vehicle_models.*`, `mission_environment.*` parseadas pero no integradas en runtime.
5. UI web legacy deprecada; reemplazada por `--ipc stdio`.

---

## 12. Estado del Proyecto

| Aspecto | Estado |
|---|---|
| Motor de simulación core | ✅ Funcional |
| Plugin pipeline (sequential + parallel) | ✅ Funcional |
| Integración física 6-DOF (RK4, Euler, Verlet) | ✅ Funcional |
| Modo CLI | ✅ Soportado |
| Modo IPC stdio | ✅ Soportado |
| Plugin `structures` (masa/límites) | ✅ Activo (física = cero pendiente) |
| OutputManager CSV/JSON | ✅ Funcional |
| Suite de tests unitarios | ✅ Cobertura de todos los managers |
| Plugin `propulsion` | 🔄 En desarrollo |
| Secciones de config avanzadas | 🔄 Reservadas/pendientes |
| UI web | ❌ Deprecada |
