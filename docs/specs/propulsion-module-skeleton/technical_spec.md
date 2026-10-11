# Propulsion module skeleton — technical specification

Status: Current — implemented and verified; see task_list.md.

## Repository findings and design decisions

- `plugins/structures` is the active reference: domain class plus C ABI adapter.
  Root CMake owns plugin targets and puts shared libraries in `bin/plugins`.
- Environment, aerodynamics and the old propulsion implementation are deprecated.
  Reusing the old propulsion code would reintroduce fuel/state mutation and physics
  outside this task. It is retained only as historical context.
- Structures' permissive CSV loader skips malformed rows and searches parent paths.
  This module instead rejects invalid tables, reports exact locations and resolves
  relative paths against the process working directory only.
- PluginManager creates before configuring, and forwards `plugins[].parameters`.
  Consequently creation allocates only; CSV I/O belongs in configure. The existing
  `vehicle_models.propulsion_model.source` is metadata, not automatically forwarded.
- No new third-party dependency: standard C++17 for domain/CSV, existing JSON and
  FlatBuffers dependencies for the adapter, existing GTest for verification.

## Files and responsibilities

| File under plugins/propulsion | Responsibility |
| --- | --- |
| propulsion_module.h/.cpp | Typed table, atomic load, interpolation, pending physics API |
| propulsion_csv.h/.cpp | File I/O, schema and numeric validation, diagnostics |
| propulsion.cpp | C ABI lifecycle, JSON configuration, verified neutral ticks |
| data/engine_curves.csv | Synthetic fixture, copied to build/bin/data/propulsion |
| tests/test_propulsion.cpp | Domain, parser and ABI regression tests |

## CSV version 1

Exact header and column order:

```csv
ambient_pressure_pa,chamber_pressure_pa,mixture_ratio,thrust_n,isp_s,interpolation
```

| Column | Meaning / constraint |
| --- | --- |
| ambient_pressure_pa | Ambient absolute pressure, Pa, >= 0; strictly increasing |
| chamber_pressure_pa | Chamber absolute pressure, Pa, > 0; constant across rows |
| mixture_ratio | Oxidizer / fuel mass ratio, dimensionless, > 0; constant |
| thrust_n | Tabulated thrust, N, >= 0 |
| isp_s | Specific impulse, seconds, > 0 |
| interpolation | Literal `linear`, repeated on each row |

At least two rows are required. Every numeric value must be finite and fully parsed;
scientific notation and surrounding spaces/tabs are supported. Decimal separator is
`.` independent of process locale. Blank lines are ignored. LF and CRLF and an
optional UTF-8 BOM on the first line are accepted. Empty, extra or missing fields,
unsupported interpolation modes, duplicate/descending pressures, changing chamber
pressure or mixture ratio, NaN, infinity and overflow are errors.

This is a deliberately restricted CSV profile: no quoted fields, embedded commas,
multiline fields or comments. Reject these explicitly; do not claim full RFC 4180
compatibility. Schema changes require a new documented version; do not silently
infer units or rename/reorder columns. No executable routines are loaded from CSV:
`interpolation` selects a known compiled algorithm.

## Memory and algorithms

`EngineCurve` owns a vector of `PerformanceSample` rows plus fixed chamber pressure,
mixture ratio and `InterpolationMethod`. `read_engine_curve_csv` constructs a local
candidate. `PropulsionModule::load_from_csv` replaces its previous table only after
successful parsing. Errors throw `std::runtime_error` with path and line number;
C ABI entry points catch failures. Accessors return const references; references
become invalid after successful reload or destruction. Configuration and ticks on
the same instance must not run concurrently, matching the host lifecycle.

`interpolate_performance(ambient_pressure_pa)` uses binary search and piecewise
linear interpolation of thrust and ISP. Exact endpoints are accepted. Empty data,
nonfinite input or out-of-domain pressure return `std::nullopt`; no extrapolation
or silent clamping. Parsing is O(n), lookup O(log n), storage O(n).

`compute_thrust_n` and `compute_mass_flow_kg_s` accept an `EngineOperatingPoint`
(ambient pressure and throttle) and return `std::nullopt` in this increment. They
are future cycle integration seams. Interpolated data do not drive runtime forces.

## Host integration and errors

`plugin_create_instance` allocates an unconfigured instance; allocation failure
returns null. `plugin_configure` accepts a JSON object with optional string
`engine_curves_path` (default `data/propulsion/engine_curves.csv`). Unknown keys are
ignored for compatibility with host parameters. Explicit `{}` loads the default;
if host configuration omits parameters entirely, the host does not call configure
and tick reports uninitialized. A failed reconfiguration preserves prior readiness.

Return codes: `0` success; `-1` invalid arguments; `-2` unconfigured tick;
`-3` invalid JSON/configuration types or invalid tick buffer/delta time;
`-4` CSV loading failure. Configuration failure reports its diagnostic to stderr.
No optional host logging export is introduced in this increment.

Tick requires a valid FlatBuffers GeneralState buffer with its correct size and
finite nonnegative delta_time. FlatBuffers verification precedes all state access.
Output pointers are optional per the ABI; canonical pointers take precedence over
legacy aliases. Provided outputs are reset even on errors; no state bytes change.
Destroy accepts null. No exception crosses an exported ABI entry point.

Root CMake registers `propulsion`, copies the fixture with the plugin target, and
registers `propulsion_tests` independently from `core_tests` to avoid collisions
between plugins' identical exported symbol names. Default config includes a disabled
type-1 plugin with explicit CSV path. Users may enable it for lifecycle integration;
it supplies no physical thrust in this stage.

## Research and improvements

- [RFC 4180](https://www.rfc-editor.org/rfc/rfc4180) describes CSV records, headers,
  quoting and equal field counts. Our numeric profile deliberately restricts quoting
  and accepts LF as well as CRLF; strict field counts prevent shifted data.
- [NASA: Specific Impulse](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/specific-impulse/)
  establishes ISP in seconds and its relationship to thrust and propellant flow.
  Future solver work may use `mass_flow = thrust / (Isp * g0)`; no such physical
  calculation or Merlin accuracy claim is part of this increment.
- [NASA: Rocket Thrust Equation](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/rocket-thrust-equation/)
  identifies ambient pressure as a thrust input. A single pressure axis at fixed
  chamber conditions is a scoped software design decision, not a universal engine model.

Future increments must decide table axes for throttle/chamber pressure/mixture ratio,
real engine dataset provenance, extrapolation policies, engine state transitions and
ownership of fuel consumption in the sequential vs. parallel host stages. A database
reader can later produce the same typed table without coupling SQL to the solver.

## Verification strategy

Build the actual root targets. Test valid initialization, exact/intermediate/boundary
lookup, unsupported range, parsing formats, malformed rows/numbers, ordering and
fixed operating conditions, failed reload preservation, missing files and empty tables.
Exercise C ABI create/configure/tick/destroy, invalid JSON and buffers, aliases,
null outputs, instance independence and state byte preservation. Run the existing
suite once to detect integration regressions; record commands and limitations in
`task_list.md`. Compile test source under the module directory as requested.
