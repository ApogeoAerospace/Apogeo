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

Back to: [`docs/README.md`](./README.md)

## Reference implementation: `example_plugin`

`plugins/example_plugin/` is the plugin to copy when writing a new one. It is built with the
other plugins (`PLUGIN_NAMES` in `CMakeLists.txt`) and demonstrates each part of this API:

| Capability | Where / how |
|---|---|
| Lifecycle | `plugin_create_instance`, `plugin_configure`, `plugin_tick`, `plugin_destroy_instance` |
| JSON configuration | Strict readers: a wrong type is rejected (`plugin_configure` returns `-3`) and a rejected configuration leaves the instance unchanged |
| Host logger | `plugin_set_host_services`; falls back to `stdout` when the host logger is off (`use_host_logger: false`) |
| Reading the state | `GeneralState`: `total_mass`, `velocity`, `wind_velocity`, `atm_density`, `gravity`, `position`, `sim_time` |
| Force output | Parallel mode (`"type": 1`): writes `output_force` / `force_out` in newtons |
| Gravity | Turns `GeneralState.gravity` into a force (see below) |
| State editing | Sequential mode (`"type": 0`): `enable_state_nudge` mutates the position through `GetMutableGeneralState` |
| Telemetry | One INFO line every `log_interval_ticks`; per-tick DEBUG lines with `debug_output` |

The plugin tells the two modes apart by the force pointers: the host passes them only to
parallel plugins. Parallel plugins must treat the state buffer as read-only.

### Parameters

All parameters are optional; the defaults leave the trajectory untouched.

| Key | Type | Default | Meaning |
|---|---|---|---|
| `use_host_logger` | bool | `true` | Read by the host, not by the plugin: share the host logger |
| `debug_output` | bool | `false` | Log every tick at DEBUG level |
| `log_interval_ticks` | integer > 0 | `100` | Ticks between INFO telemetry lines |
| `apply_gravity` | bool | `false` | Apply `F = m * g * gravity_scale` using `GeneralState.gravity` (parallel mode) |
| `gravity_scale` | number >= 0 | `1.0` | Multiplier of the gravity force |
| `drag_coefficient` | number >= 0 | `0.0` | Quadratic drag `F = -1/2 * rho * Cd * A * norm(v_rel) * v_rel`; `0` disables it |
| `reference_area` | number > 0 | `0.5` | Reference area `A` in m^2 used by the drag model |
| `enable_state_nudge` | bool | `false` | Sequential mode only: move the position along +X by `step_size` per tick |
| `step_size` | number | `1.0` | Metres per tick for `enable_state_nudge` |

`v_rel` is the vehicle velocity minus `wind_velocity`, so drag follows the wind in the state.

### Gravity

The core does not apply gravity: it carries `GeneralState.gravity` and writes it to the output.
With `apply_gravity` this plugin shows the intended pattern for a module that consumes the
state vector. Enable **only one** gravity provider: the legacy Environment module computes its
own gravity (ignoring the state vector), so enabling both applies gravity twice. The
[web panel](./WEB_CONTROL_PANEL.md#gravity) warns about this.

Example, gravity plus drag for a parallel plugin:

```json
{
  "name": "example_plugin",
  "type": 1,
  "library_path": "plugins/example_plugin",
  "enabled": true,
  "parameters": { "apply_gravity": true, "drag_coefficient": 0.3, "reference_area": 0.8 }
}
```

The physics lives in `example_physics.h` (standard library only) so it can be unit tested
without the simulator: `tests/test_example_plugin_physics.cpp`.
