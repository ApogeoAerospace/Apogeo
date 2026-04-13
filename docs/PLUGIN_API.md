# Plugin API

## Types

- `PluginHandle`: opaque plugin-instance handle.
- `PluginVector3`: `{ float x, y, z }`.
- `PluginTickData`:
  - `uint8_t* state_buffer`
  - `uint32_t buffer_size`
  - `double delta_time`
  - `PluginVector3* output_force`
  - `PluginVector3* output_torque`
  - `PluginVector3* force_out`
  - `PluginVector3* torque_out`
- `PluginLogLevel`:
  - `PLUGIN_LOG_DEBUG`
  - `PLUGIN_LOG_INFO`
  - `PLUGIN_LOG_WARNING`
  - `PLUGIN_LOG_ERROR`
  - `PLUGIN_LOG_CRITICAL`
- `PluginLogFn`:
  - C callback for plugin log emission through host.
- `PluginHostServices`:
  - `uint32_t api_version`
  - `PluginLogFn log`
  - `void* user_data`

## Required exports

- `plugin_create_instance()`
- `plugin_tick(PluginHandle, PluginTickData*)`
- `plugin_destroy_instance(PluginHandle)`

## Optional export

- `plugin_configure(PluginHandle, const char* json_params)`
- `plugin_set_host_services(const PluginHostServices* services)`

## Runtime behavior

- Sequential plugins can mutate `state_buffer`.
- Parallel plugins compute output force/torque.
- `plugin_configure` is invoked when plugin parameters are provided.
- `plugin_set_host_services` is invoked by host only if:
  - plugin exports the optional symbol, and
  - `plugins[].parameters.use_host_logger == true` in configuration.

## Host-plugin logging integration

- Host registers an internal logging bridge in `PluginManager` and injects it through `PluginHostServices`.
- If a plugin does not implement `plugin_set_host_services`, it remains compatible.
- If `use_host_logger` is not enabled for a plugin, no services are injected and plugin may use local fallback.
- This integration preserves modular boundaries: C contract in `plugin_api.h` with no direct `Logger.h` dependency inside plugins.
- Plugins should validate `PluginHostServices.api_version` before consuming host callbacks.
- Current `structures` plugin behavior: if host `api_version` is incompatible, it emits a warning and falls back to local plugin logging (host services disabled for that plugin instance).

---

Documentation status: **Current**

Back to: [`docs/README.md`](../README.md)
