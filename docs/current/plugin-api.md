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

## Exportaciones requeridas

- `plugin_create_instance()`
- `plugin_tick(PluginHandle, PluginTickData*)`
- `plugin_destroy_instance(PluginHandle)`

## Exportación opcional

- `plugin_configure(PluginHandle, const char* json_params)`

## Comportamiento en runtime

- Los plugins secuenciales pueden mutar `state_buffer`.
- Los plugins paralelos calculan fuerza/torque de salida.
- `plugin_configure` se invoca cuando hay parámetros de plugin.

---

Estado de documentación: **Current**

Volver a: [`docs/README.md`](../README.md)
