#ifndef MOLAB_NAMESPACE_DOCS_H
#define MOLAB_NAMESPACE_DOCS_H

/**
 * @file molab_namespace_docs.h
 * @brief Documentación de alto nivel del namespace principal del proyecto.
 */

/**
 * @namespace MoLab
 * @brief Namespace principal del núcleo del simulador MoLab.
 *
 * Agrupa los componentes principales de simulación, gestión de plugins,
 * configuración, logging, tiempo y salida de resultados.
 *
 * Componentes destacados:
 * - `SimulationEngine`: orquestación del ciclo de simulación.
 * - `PluginManager`: carga/ejecución de plugins dinámicos.
 * - `PhysicsIntegrator`: integración numérica del estado.
 * - `ConfigManager`: configuración global y por ejecución.
 * - `TimeManager`: manejo de tiempo de simulación y UTC.
 * - `OutputManager`: persistencia y exportación de estados.
 * - `Logger`: sistema de logging del núcleo.
 */
namespace MoLab {}

#endif // MOLAB_NAMESPACE_DOCS_H
