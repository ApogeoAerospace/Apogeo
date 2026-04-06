# State Vector Model

\page state_vector_model State Vector Model

This page describes the serialized simulation state model used by MoLab.
The model is defined in `src/schemas/state_vector.fbs` and generated to C++.

## Namespace

All generated types are under `state_vector`.

## Main types

- `Vec3`: vector 3D (`x`, `y`, `z`)
- `Quaternion`: attitude quaternion (`x`, `y`, `z`, `w`)
- `InertiaTensor`: compact representation of inertia tensor
- `EngineCmd`: per-engine command (throttle and TVC)
- `GeneralState`: root state table

## `GeneralState` sections

### Time / Integrator
- `sim_time`
- `dt`

### Kinematics
- `position`
- `velocity`
- `orientation`
- `angular_velocity`

### Dynamics / Mass properties
- `total_mass`
- `cg_location`
- `inertia_tensor`
- `propellant_masses`

### Aerodynamics
- `mach_number`
- `dynamic_pressure`
- `angle_of_attack`
- `sideslip_angle`

### Environment
- `atm_density`
- `atm_pressure`
- `atm_temperature`
- `wind_velocity`
- `gravity`

### Actuation / Control
- `engines`
- `surface_deflections`

## Notas

- `GeneralState` is the root schema type (`root_type GeneralState`).
- The schema is consumed by core modules such as `InitialStateLoader`,
  `PhysicsIntegrator`, `PluginManager`, and `SimulationEngine`.
