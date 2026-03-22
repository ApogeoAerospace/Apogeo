# Plan del plugin de estructuras

## Objetivo

Implementar manejo de masa/inercia y límites estructurales.

## Modo

Preferir `type=0` si se muta el estado directamente; usar `type=1` solo para contribuciones de fuerza basadas en cargas.

## Hitos

1. Depleción de propelente -> actualización de masa total
2. Actualización de CG + actualización de tensor de inercia
3. validaciones de límites estructurales y advertencias/eventos

## Claves de configuración (planificadas)

- URI de la fuente de propiedades de masa
- parámetros del modelo de tanques
- límites de carga admisibles

## Pruebas

- la masa disminuye según lo esperado
- el tensor de inercia se mantiene físicamente válido
- exceder límites dispara el comportamiento esperado

---

Estado de documentación: **Current**

Tipo de documento: **Plan (roadmap)**

Volver a: [`docs/README.md`](../README.md)
