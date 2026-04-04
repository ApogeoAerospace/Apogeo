# Plan del plugin de propulsión

## Objetivo

Implementar plugin de propulsión (`type=1`) con empuje + flujo de masa.

## Entradas

- comando de throttle
- archivos de curvas de motor
- presión/altitud atmosférica

## Salidas

- vector de fuerza de empuje
- torque opcional por offset de gimbal

## Hitos

1. Empuje constante de motor único
2. Interpolación de curvas de throttle + ISP
3. Soporte multi-motor + gimbal
4. modos de fallo/de-rate

## Claves de configuración (planificadas)

- `engine_count`
- `thrust_curve_uri`
- `isp_curve_uri`
- `throttle_limits`
- `gimbal_limits_deg`

## Pruebas

- continuidad de empuje en el rango de throttle
- flujo de masa esperado según relación con ISP
- consistencia de signo/eje de torque

---

Estado de documentación: **Current**

Tipo de documento: **Plan (roadmap)**

Volver a: [`docs/README.md`](../README.md)
