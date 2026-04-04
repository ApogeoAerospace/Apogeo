# Fase actual del Modulo Structures 

## 1. Objetivo de este documento

Este documento se deja trazabilidad tecnica completa del modulo Structures desde su implementacion inicial en esta rama hasta el estado actual.

Incluye:
- Flujo real de ejecucion en MoLab y como se conecta Structures.
- Diferencia operativa entre plugins tipo 0 y tipo 1.
- Uso de datos por tick (lectura/escritura) y contratos de API.
- Que hace cada archivo `.h` y `.cpp` involucrado.
- Cambios arquitectonicos clave (CMake, ConfigManager, deprecados, actuadores).
- Reglas de distribucion de responsabilidades entre plugins.
- Plan de migracion recomendado para cierre de transicion de actuadores.

## 2. Flujo real del simulador y posicion de Structures

### 2.1 Flujo de arranque

1. `main.cpp` carga argumentos CLI y llama a `SimulationEngine::initialize_with_config()`.
2. `ConfigManager` lee JSON de configuracion y construye `plugin_configs_`.
3. `PluginManager::load_plugins_from_config()` carga DLL/SO de cada plugin y ejecuta `plugin_configure()` con `parameters`.
4. `SimulationEngine` carga el estado inicial FlatBuffer y comienza ticks.

### 2.2 Flujo por tick

En cada tick, `PluginManager::run_simulation_cycle()` corre tres fases:

1. **Fase secuencial (tipo 0)**
   - Metodo: `execute_sequential_plugins()`.
   - Plugins pueden modificar `state_buffer` directamente.
   - No se esperan fuerzas/torques de salida.

2. **Fase paralela de fisica (tipo 1)**
   - Metodo: `execute_parallel_plugins()`.
   - Plugins leen estado y devuelven `output_force`/`output_torque`.
   - Se acumulan todas las contribuciones.

3. **Integracion fisica**
   - Metodo: `apply_physics_integration()`.
   - `PhysicsIntegrator` integra estado con fuerza/torque totales.

Structures esta implementado como **tipo 1** (calculador de contribucion fisica, no modificador secuencial del estado).

## 3. Contrato de plugin y uso de datos

## 3.1 API comun (`src/api/plugin_api.h`)

Cada plugin exporta:
- `plugin_create_instance()`
- `plugin_configure(handle, json_params)`
- `plugin_tick(handle, PluginTickData*)`
- `plugin_destroy_instance(handle)`

`PluginTickData` entrega:
- `state_buffer` + `buffer_size`
- `delta_time`
- Salidas de fuerza/torque por `output_force` y `output_torque`
- Alias legacy `force_out` y `torque_out` (compatibilidad)

## 3.2 Diferencia exacta tipo 0 vs tipo 1

- **Tipo 0 (SEQUENTIAL_STATE_MODIFIER)**
  - Se ejecuta en serie.
  - Puede mutar el estado FlatBuffer.
  - Ejemplo: `plugins/example_plugin/example_plugin.cpp` modifica posicion X.

- **Tipo 1 (PARALLEL_PHYSICS_CALCULATOR)**
  - Se ejecuta en paralelo.
  - Debe tratar `state_buffer` como lectura para calculo.
  - Escribe salida solo en `output_force`/`output_torque`.

Structures usa el patron tipo 1: lee estado, calcula fuerza/torque estructurales, reporta alertas de integridad cuando aplica.

## 4. Estado funcional actual de Structures

## 4.1 Comportamiento 

- Carga masa/CoM/inercia desde JSON placeholder (`mass_properties.json`).
- Carga limites estructurales desde CSV (`structural_limits.csv`).
- En `plugin_tick` calcula:
  - velocidad,
  - aceleracion longitudinal aproximada,
  - `g_force`,
  - presion dinamica aproximada,
  - chequeo de integridad estructural,
  - fuerza y torque de amortiguamiento estructural.

## 4.2 Ajuste de primer tick (warm-up)

Para evitar falsos picos de `g` en el primer tick:
- Se inicializa `previous_speed_m_s` con velocidad actual en tick 1.
- No se evalua derivada de velocidad en tick 1.
- La validacion de integridad con `g`/`q` se aplica desde tick >= 2.

## 4.3 Cambios de actuadores (temporal)

Punto clave para futuros mantenedores:

- En la arquitectura los, **actuadores deben pertenecer al plugin Programming**.
- Structures **ya no depende funcionalmente** de `params["actuators"]` para su fisica, como se tenía inicialmente.
- En `StructuresModule` se conserva API legacy:
  - `setActuatorsFromJsonArray(...)`
  - `mapActuatorsPlaceholder()`
- Estas funciones quedan como compatibilidad transitoria/no-op fisico mientras se construye el plugin programming.

Consecuencia:
- Configs antiguas con bloque de actuadores no deben romper, pero Structures no debe tomar decisiones fisicas basadas en ese bloque.

## 5. Responsabilidad por archivo (.h/.cpp)

## 5.1 Estructuras del modulo Structures

### `plugins/temp/structures/structures_module.h`

Define dominio y contrato interno de Structures:
- Tipos de datos (`Vec3d`, `InertiaTensorData`, `StructuralLimits`).
- Clase `StructuresModule` y metodos publicos para:
  - carga de masa/limites,
  - chequeo de integridad,
  - calculo de fuerza/torque.
- Declara APIs de actuadores como **legacy/transicion**.
- Almacena `actuators_raw_` solo para compatibilidad temporal.

### `plugins/temp/structures/structures_module.cpp`

Implementacion del dominio Structures:
- Parse JSON de masa/inercia placeholder.
- Parse CSV de limites estructurales.
- `setActuatorsFromJsonArray`: guarda JSON crudo (compatibilidad, sin efecto fisico).
- `mapActuatorsPlaceholder`: no-op intencional para mantener compatibilidad de flujo.
- `computeStructuralForce` y `computeStructuralTorque`: amortiguamiento simple basado en velocidad y velocidad angular.

### `plugins/temp/structures/structures.cpp`

Capa plugin/API (orquestacion):
- Maneja ciclo de vida exportado (`create/configure/tick/destroy`).
- Carga defaults al crear instancia.
- En `plugin_configure` toma parametros estructurales (`mass_properties_path`, `structural_limits_path`, `debug_output`).
- Ejecuta logica de tick tipo 1, incluyendo warm-up del primer tick.
- Reporta warning cuando integridad excede limites y `debug_output == true`.

## 5.2 Archivos del core relevantes para Structures

### `src/core/ConfigManager.cpp`

Responsabilidad en este momento:
- Parseo generico de seccion `plugins[]`.
- No realiza inyecciones especiales de datos de Structures desde `vehicle_models`.

Razon:
- Evitar duplicacion de parseo core/plugin.
- Mantener interpretacion de parametros en cada plugin dueño de esa logica.

### `src/core/PluginManager.cpp`

Responsabilidad:
- Carga bibliotecas dinamicas.
- Ejecuta `plugin_configure` con `parameters` por plugin.
- Orquesta fases tipo 0, tipo 1, integracion.
- Acumula fuerzas/torques de plugins tipo 1.

### `src/core/SimulationEngine.cpp`

Responsabilidad:
- Inicializacion global con config.
- Ejecucion de ticks.
- Uso de `PluginManager` como scheduler de plugins.

### `src/main.cpp`

Responsabilidad:
- Entrada CLI, seleccion de config, arranque y cierre del motor.

## 5.3 Configuracion y build

### `data/defaults/default_config.json`

- Define plugins activos, tipo y parametros.
- Structures esta configurado como tipo 1.
- Actualmente expone `debug_output` dentro de `plugins[].parameters`.
- Existen datos de `vehicle_models.physical_limits.actuators`, pero ya no se inyectan automaticamente a Structures desde core.

### `CMakeLists.txt` (raiz)

Cambio importante aplicado:
- Structures fue integrado al bloque normal de `PLUGIN_NAMES`.
- Para `structures`, las fuentes son:
  - `plugins/temp/structures/structures.cpp`
  - `plugins/temp/structures/structures_module.cpp`
- Se agrego include especifico de carpeta Structures y link a `nlohmann_json`.

Impacto que se espera tener:
- Build consistente con resto de plugins.
- Menos logica  dispersa.

### `plugins/temp/structures/CMakeLists.txt`

Existe como CMake local historico, pero **la compilacion activa del repo la controla el CMake raiz**.
Se mantiene como referencia local del plugin, no como fuente principal de verdad del build global.

## 6. Historial de implementacion (desde inicio)

Esta secuencia resume lo implementado en esta iteracion del modulo:

1. Se analizo el flujo completo de MoLab y se definio Structures como plugin tipo 1.
2. Se paso de una implementacion mas monolitica a separacion por capas:
   - `structures.cpp` (API/orquestacion)
   - `structures_module.h/.cpp` (dominio).
3. Se agrego carga de entradas externas:
   - JSON de propiedades de masa
   - CSV de limites estructurales.
4. Se incorporaron logs de trazabilidad para creacion/configuracion/limites.
5. Se corrigio pico falso de `g` en primer tick con estrategia warm-up.
6. Se ajusto CMake para integrar Structures en el flujo estandar de plugins (Option A).
7. Se removio inyeccion especial desde `ConfigManager` para evitar parseo duplicado.
8. Se dejo copia de implementacion anterior en deprecados.
9. Se marco manejo de actuadores en Structures como transicional/compatibilidad, migrando autoridad a Programming.

## 7. Deprecated 

### `data/deprecated/plugin/structures/structures.cpp`

- Conserva implementacion anterior (referencia historica).
- Incluye guardas de compilacion para no romper workspaces sin headers generados.
- No participa en build principal.


## 8. Distribucion correcta de responsabilidades entre plugins

La separacion de responsabilidades se definio para evitar que un mismo dato de dominio se procese en multiples capas con reglas distintas. El plugin Programming es el unico responsable funcional del dominio de actuadores: recibe comandos, aplica validaciones y restricciones (saturacion por rango, limitacion por tasa, estados invalidos) y publica el resultado efectivo de actuacion en el estado compartido. Esto significa que Programming no solo "pasa" comandos, sino que normaliza y deja una salida canonica para que el resto del sistema consuma una unica fuente de verdad.

En concreto, Programming concentra estas decisiones:
- Validacion de comandos de entrada.
- Aplicacion de limites fisicos de actuacion.
- Publicacion del estado aplicado de actuadores como salida canonica.

Structures, por su parte, debe permanecer como consumidor estructural de ese estado ya resuelto. Su objetivo es solo evaluar consecuencias estructurales: cargas, limites, integridad y contribucion de fuerza/torque asociada al modelo estructural que le corresponda. Mientras la integracion completa de actuadores en buffer termina de consolidarse, Structures mantiene APIs legacy de compatibilidad para no romper configuraciones antiguas, pero esas APIs no deben volver a convertirse en una autoridad de logica fisica. De esta manera, lo que queremos es: Structures puede leer, interpreta, pero no redefinir reglas de actuacion que pertenecen a Programming.

En otras palabras, en Structures:
- Si se recibe configuracion legacy de actuadores, se tolera por compatibilidad.
- Esa configuracion legacy no debe alterar decisiones fisicas nuevas.
- La evaluacion estructural debe depender del estado compartido canonico.

Segun los cambios, el ConfigManager debe parsear de forma generica y entregar `parameters` por plugin sin reinterpretar semantica de dominio. PluginManager debe cargar, configurar y ejecutar plugins respetando el ciclo de simulacion (fase secuencial tipo 0, fase paralela tipo 1, integracion), sin mezclar reglas de negocio de actuadores, estructuras, propulsion o aerodinamica. Cuando el Core absorbe semantica de un dominio especifico, se crea acoplamiento fuerte y duplicacion de parseo; por eso se elimino la inyeccion especial previa para Structures.

El Core debe limitarse a:
- Parseo generico de configuracion.
- Carga/configuracion/ejecucion de plugins.
- Orquestacion del ciclo y acumulacion de salidas.

Lo que se quiere es:
- Menor acoplamiento entre plugins.
- Menos duplicacion de parseo y validacion.
- Diagnostico mas rapido por ownership claro.

En resumen, Programming publica estado aplicado de actuacion en el buffer; Structures consume ese estado para evaluacion estructural; Core permanece sin saber el significado fisico de esos campos.



---
Fecha de actualizacion: 2026-04-04.
