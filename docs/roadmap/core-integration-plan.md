# Plan de integración del núcleo

## Alcance

Integrar en runtime las secciones de configuración que actualmente no se usan.

## Ítems de trabajo

1. Extender `ConfigManager` con estructuras tipadas:
   - `LoggingConfig`
   - `PerformanceConfig`
   - `ValidationConfig`
   - `MissionEnvironmentConfig`
   - `VehicleModelsConfig`

2. Aplicar integración en runtime:
   - `output_directory` -> `OutputManager::setOutputDirectory(...)`
   - `simulation.enable_logging` -> switch global de emisión de logs
   - `logging.*` -> consola/archivo/rotación
   - `performance.thread_pool_size` -> tamaño del scheduler de plugins
   - `validation.*` -> política de umbrales/validaciones en runtime

3. Integración del integrador:
   - `mission_environment.integrator_config.method/rtol/atol` -> `PhysicsIntegrator`

4. Agregar pruebas:
   - pruebas de parseo de configuración
   - pruebas de comportamiento en runtime
   - pruebas de modos de fallo

## Hitos

- M1: parseo + getters
- M2: integración en runtime
- M3: pruebas + sincronización de documentación

---

Estado de documentación: **Current**

Tipo de documento: **Plan (roadmap)**

Volver a: [`docs/README.md`](../README.md)
