# Structures Module (current implementation)

This document describes the current behavior of the Structures plugin/module implemented in:

- `plugins/structures/structures.cpp`
- `plugins/structures/structures_module.h`
- `plugins/structures/structures_module.cpp`

## Scope

The module currently provides:

- Loading mass/inertia placeholder data from JSON.
- Loading structural limit values from CSV.
- Runtime integrity checks against configured limits.
- Plugin host logger integration (optional).

## Data loading

### Mass properties (`JSON`)

`StructuresModule::loadMassPropertiesFromJson(...)` reads:

- `initial_total_mass_kg`
- `fuel_mass_kg`
- `center_of_mass_m.{x,y,z}`
- `inertia_tensor_kg_m2.{ixx,iyy,izz,ixy,ixz,iyz}`

If fields are missing, existing defaults are retained.

### Structural limits (`CSV`)

`StructuresModule::loadStructuralLimitsFromCsv(...)` reads key/value rows for:

- `max_g_load`
- `warning_g_load`
- `max_dynamic_pressure`
- `warning_dynamic_pressure`

Unknown rows are ignored.

### Path resolution behavior

Both loaders resolve paths through a fallback search strategy:

- absolute path (if existing)
- original relative path
- `cwd / relative path`
- parent-directory walk-up candidates (up to 6 levels)

## Plugin lifecycle

### `plugin_create_instance`

- Creates `StructuresPluginInstance`.
- Loads default files:
  - `data/mass/mass_properties.json`
  - `data/limits/structural_limits.csv`
- Returns `nullptr` on loading failure.

### `plugin_configure`

Accepts JSON parameters:

- `mass_properties_path` (`string`, optional)
- `structural_limits_path` (`string`, optional)
- `debug_output` (`bool`, optional)

Returns:

- `0` on success
- negative error code on invalid input or loading/parsing failure

### `plugin_tick` (current runtime behavior)

Current implementation validates inputs/state and outputs **zero force** and **zero torque**.

This keeps interface compatibility while the final structural physics model is pending.

## Host logger integration

If host services are provided via `plugin_set_host_services(...)`, plugin logs are forwarded through host logger callback. Otherwise, the plugin falls back to local `stdout` logging.

Host injection is controlled by runtime plugin configuration (`use_host_logger` / host logger integration flag in plugin parameters).
Tick-level verbosity is controlled by the host logger level (for example `DEBUG`), not by plugin-local debug flags.

Compatibility behavior:

- Structures validates `PluginHostServices.api_version` before enabling host logger callback usage.
- Supported host-services API version: `1`.
- On mismatch, Structures emits a warning and falls back to local plugin logging (host logger disabled for the plugin).

## Notes

- `StructuresModule::computeStructuralForce(...)` and `computeStructuralTorque(...)` currently contain temporary damping templates in module code, but `plugin_tick` currently emits neutral outputs.
- Structural checks are available through `checkStructuralIntegrity(...)` using loaded limit values.

---

Documentation status: **Current**
