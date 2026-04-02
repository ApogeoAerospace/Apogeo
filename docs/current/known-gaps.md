# Brechas conocidas (actual)

## Brechas de integración configuración/runtime

1. `output_directory` se parsea, pero la salida del motor sigue usando una ruta hardcodeada.
2. `simulation.enable_logging` se parsea, pero no se usa como interruptor global on/off.
3. La sección `logging.*` no está integrada con el backend de logging.
4. La sección `performance.*` no está integrada con controles de scheduler/métricas.
5. Los umbrales de `validation.*` no están integrados en la validación runtime.
6. `vehicle_models.*` no se usa.
7. `mission_environment.*` no se usa.

## Brechas de contrato UI/backend

- La UI web envía secciones de física/salida que el `ConfigManager` actual no parsea.
- Diferencia de puertos:
  - mensaje del lanzador: `8080`
  - servidor Python: `8082`

---

Estado de documentación: **Current**

Volver a: [`docs/README.md`](../README.md)
