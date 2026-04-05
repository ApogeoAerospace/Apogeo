# API de plugins

## Tipos

- `PluginHandle`: handle opaco de instancia de plugin.
- `PluginVector3`: `{ float x, y, z }`.
- `PluginTickData`:
  - `uint8_t* state_buffer`
  - `uint32_t buffer_size`
  - `double delta_time`
  - `PluginVector3* output_force`
  - `PluginVector3* output_torque`
  - `PluginVector3* force_out` (compatibilidad deprecada)
  - `PluginVector3* torque_out` (compatibilidad deprecada)
- `PluginLogLevel`:
  - `PLUGIN_LOG_DEBUG`
  - `PLUGIN_LOG_INFO`
  - `PLUGIN_LOG_WARNING`
  - `PLUGIN_LOG_ERROR`
  - `PLUGIN_LOG_CRITICAL`
- `PluginLogFn`:
  - Callback C para que el plugin emita logs a través del host.
- `PluginHostServices`:
  - `uint32_t api_version`
  - `PluginLogFn log`
  - `void* user_data`

## Exportaciones requeridas

- `plugin_create_instance()`
- `plugin_tick(PluginHandle, PluginTickData*)`
- `plugin_destroy_instance(PluginHandle)`

## Exportación opcional

- `plugin_configure(PluginHandle, const char* json_params)`
- `plugin_set_host_services(const PluginHostServices* services)`

## Comportamiento en runtime

- Los plugins secuenciales pueden mutar `state_buffer`.
- Los plugins paralelos calculan fuerza/torque de salida.
- `plugin_configure` se invoca cuando hay parámetros de plugin.
- `plugin_set_host_services` se invoca por el host solo si:
  - el plugin exporta el símbolo opcional, y
  - `plugins[].parameters.use_host_logger == true` en configuración.

## Integración de logging host-plugin

- El host registra un bridge de logging interno en `PluginManager` y lo inyecta al plugin mediante `PluginHostServices`.
- Si un plugin no implementa `plugin_set_host_services`, sigue siendo compatible.
- Si `use_host_logger` no está activo para un plugin, no se inyectan servicios y el plugin puede usar su fallback local.
- Esta integración mantiene el límite modular del sistema: contrato C en `plugin_api.h` sin dependencia directa de `Logger.h` dentro de los plugins.

---

Estado de documentación: **Current**

Volver a: [`docs/README.md`](../README.md)
