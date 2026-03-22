# Documentación de MoLab

Este directorio contiene la documentación del proyecto MoLab.

## Estructura

La documentación está dividida en tres rutas explícitas:

1. **Current** (`docs/current/`)  
   Lo que ya está implementado en la base de código.

2. **Hoja de Ruta / Planes de Implementación** (`docs/roadmap/`)  
   Funcionalidades y módulos planificados pero no implementados completamente.

3. **Legacy** (`docs/deprecated/`)  
   Documentos heredados conservados para trazabilidad histórica.

## Inicio rápido (implementación actual)

- [`Compilación y ejecución (actual)`](./current/build-run.md)
- [`Arquitectura actual`](./current/architecture.md)
- [`Contrato de configuración actual`](./current/config.md)
- [`API de plugins`](./current/plugin-api.md)
- [`Brechas conocidas`](./current/known-gaps.md)
- [`Guía de estilo Doxygen`](./current/doxygen-style.md)
- [`Referencia completa de configuración`](./current/config-reference-full.md)

## Hoja de ruta

- [`Resumen de roadmap`](./roadmap/roadmap-overview.md)
- [`Plan de integración del núcleo`](./roadmap/core-integration-plan.md)
- [`Plan del plugin de aerodinámica`](./roadmap/plugin-aerodynamics-plan.md)
- [`Plan del plugin de propulsión`](./roadmap/plugin-propulsion-plan.md)
- [`Plan del plugin de estructuras`](./roadmap/plugin-structures-plan.md)
- [`Plan del plugin de ambiente`](./roadmap/plugin-environment-plan.md)

## Política de estado de documentación

Cada documento debe etiquetar explícitamente su contenido como:

- **Current**
- **Legacy**

Para planes futuros, usar `docs/roadmap/` (estado `Current`, tipo plan).

## Contribución

Consulta [`CONTRIBUTING.md`](./CONTRIBUTING.md) para lineamientos de contribución.
