# Modelo de Vector de Estado

\page state_vector_model Modelo de Vector de Estado

Esta página describe el modelo de estado de simulación serializado que utiliza MoLab.
El modelo se define en `src/schemas/state_vector.fbs` y se genera a C++.

## Namespace

Todos los tipos generados están bajo `state_vector`.

## Tipos principales

- `Vec3`: vector 3D (`x`, `y`, `z`)
- `Quaternion`: cuaternión de actitud (`x`, `y`, `z`, `w`)
- `InertiaTensor`: representación compacta del tensor de inercia
- `EngineCmd`: comando por motor (aceleración y TVC)
- `GeneralState`: tabla raíz del estado

## Secciones de `GeneralState`

### Tiempo / Integrador
- `sim_time`
- `dt`

### Cinemática
- `position`
- `velocity`
- `orientation`
- `angular_velocity`

### Dinámica / Propiedades de masa
- `total_mass`
- `cg_location`
- `inertia_tensor`
- `propellant_masses`

### Aerodinámica
- `mach_number`
- `dynamic_pressure`
- `angle_of_attack`
- `sideslip_angle`

### Entorno
- `atm_density`
- `atm_pressure`
- `atm_temperature`
- `wind_velocity`
- `gravity`

### Actuación / Control
- `engines`
- `surface_deflections`

## Notas

- `GeneralState` es el tipo raíz del schema (`root_type GeneralState`).
- El schema es consumido por módulos del núcleo como `InitialStateLoader`,
  `PhysicsIntegrator`, `PluginManager` y `SimulationEngine`.
