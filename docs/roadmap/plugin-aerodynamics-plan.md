# Plan del plugin de aerodinámica

## Objetivo

Implementar un plugin de modelo aerodinámico de fuerza/torque (`type=1`).

## Entradas

- velocidad, densidad atmosférica, mach, alpha/beta
- tablas opcionales de consulta desde `vehicle_models.aero_database`

## Salidas

- `output_force`
- `output_torque`

## Hitos

1. Modelo de arrastre constante (`Cd`, `Aref`)
2. Modelo de sustentación + fuerza lateral
3. Modelo por tabla (`alpha/beta/mach`)
4. Momentos aerodinámicos

## Claves de configuración (planificadas)

- `reference_area`
- `drag_model` (`constant`, `lookup`)
- `lift_model` (`off`, `linear`, `lookup`)
- `moment_model` (`off`, `lookup`)

## Pruebas

- velocidad cero => fuerza aerodinámica cero
- arrastre monótono con la velocidad
- regresión de interpolación en tabla

---

Estado de documentación: **Current**

Tipo de documento: **Plan (roadmap)**

Volver a: [`docs/README.md`](../README.md)
