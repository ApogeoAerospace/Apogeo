# Contrato de Persistencia y Restauración de Estado (Save / Load)

Este documento define el contrato formal de persistencia del simulador aeroespacial **Apogeo**, detallando qué componentes participan en el guardado y restauración de una simulación, cómo se gestiona el versionado de esquemas y las limitaciones de cada subsistema.

---

## 1. Alcance: Componentes con Estado Persistente

| Componente | Tipo de Estado | Participa en Save/Load | Descripción y Campos Guardados |
| :--- | :--- | :---: | :--- |
| **`SimulationEngine`** | Central / Cinemático | **Sí** | Vector FlatBuffers completo codificado en Base64 (`GeneralState`), tiempo acumulado (`simulation_time_`), conteo de iteraciones (`iteration_count_`) y estado de ejecución (`is_running_`). |
| **`TimeManager`** | Temporal | **Sí** | `simulation_time_` y tiempo base `utc_start_time_` (con precisión `double`). |
| **`FlightComputerContext` (Lua)** | Lógica de Vuelo | **Sí** | Variables globales de tipos primitivos (`number`, `boolean`, `string`) y tablas de datos anidadas/listas. |
| **Plugin `structures`** | Físico / Estructural | **Sí** | Masa de combustible (`fuel_mass_kg`), vector centro de masa (`center_of_mass`: x, y, z) y tensor de inercia (`inertia_tensor`: ixx, iyy, izz, ixy, ixz, iyz). |
| **Plugin `aerodynamics`** | Aerodinámica tabular | **No** | Es **stateless** en runtime; se recarga de la base de datos HDF5 definida en la configuración original. |
| **Plugin `propulsion`** | Propulsión tabular | **No** | Es **stateless** en runtime; se re-inicializa desde las curvas CSV configuradas. |
| **Métricas de Ejecución** | Rendimiento / I/O | **No** | Tiempos de tick (`compute_tick_duration_ms_`, `io_tick_duration_ms_`) y contadores de diagnóstico no afectan la física ni la trayectoria; se reinician al reanudar. |

---

## 2. Contrato de la API C de Plugins (`plugin_api.h`)

Para participar en la persistencia, un plugin dinámico exporta 4 funciones opcionales:

```c
// Consulta de capacidades y versión de esquema
PLUGIN_EXPORT char* plugin_get_persistence_info(PluginHandle handle);

// Serialización del estado a JSON
PLUGIN_EXPORT char* plugin_save_state(PluginHandle handle);

// Deserialización y restauración desde JSON
PLUGIN_EXPORT int32_t plugin_load_state(PluginHandle handle, const char* state_json);

// Liberación de memoria asignada por el plugin para strings devueltos
PLUGIN_EXPORT void plugin_free_state_string(PluginHandle handle, char* state_str);
```

### Reglas del Contrato de Plugins:
1. **Declaración explícita de soporte:** `plugin_get_persistence_info` devuelve una cadena JSON en el formato:
   ```json
   {"supports_persistence": true, "schema_version": 1}
   ```
2. **Plugins Stateless:** Si un plugin no exporta estas funciones o devuelve `supports_persistence: false`, el host no guardará ni exigirá estado para él durante la carga.
3. **Versión de esquema:** Toda modificación en los campos persistidos de un plugin debe incrementar `schema_version`. El host rechazará cualquier archivo de guardado cuya versión no coincida exactamente con la del plugin cargado.
4. **Liberación de memoria:** El host siempre invoca `plugin_free_state_string` para liberar las cadenas retornadas por `plugin_save_state` y `plugin_get_persistence_info`.

---

## 3. Contrato del Flight Computer (Lua)

El intérprete de Lua integrado (`sol::state`) almacena el estado de los scripts de vuelo del usuario con las siguientes reglas:

1. **Variables Persistidas:**
   - Tipos primitivos: `number` (enteros y números en punto flotante), `boolean` (`true` / `false`) y `string` (cadenas de texto).
   - **Tablas de datos:** Se serializan recursivamente tablas asociativas (diccionarios), tablas indexadas secuencialmente (listas/vectores de 1..N) y estructuras anidadas.
2. **Variables Excluidas:** Se ignoran deliberadamente:
   - Funciones (incluyendo callbacks como `on_tick` y funciones anidadas dentro de tablas).
   - Bibliotecas del sistema y sandbox de Apogeo (`math`, `get_altitude`, `set_throttle`, `print_log`, etc.).
   - Metatablas, hilos (coroutines) y `userdata`.
   - Referencias circulares en tablas (detectadas automáticamente y omitidas para evitar recursión infinita).
3. **Scripts Stateless:** Un script que solo implemente `on_tick(dt)` utilizando variables locales o lectura directa de telemetría es compatible con Save/Load de manera transparente (su estado guardado es un objeto vacío `{}`).

---

## 4. Política de Restauración Atómica

Para prevenir estados inconsistentes o simulaciones parcialmente cargadas:
1. **Pre-validación estricta:** Antes de mutar cualquier variable de la simulación activa:
   - Se valida el esquema general del archivo `.apogeo`.
   - Se decodifica y verifica la integridad del buffer FlatBuffers (`VerifyGeneralStateBuffer`).
   - Se verifica que todos los plugins con `supports_persistence: true` estén presentes en memoria y que sus versiones de esquema coincidan.
   - Se validan los tipos de las variables globales de Lua.
2. **Comportamiento ante fallos:** Si un plugin falla en `plugin_load_state` o falta un componente requerido, **el proceso se aborta inmediatamente sin modificar el estado actual del motor**, emitiendo un mensaje de error explicativo en los logs (`SimulationEngine` / `PluginManager`).

