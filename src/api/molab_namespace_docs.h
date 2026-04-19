#ifndef MOLAB_NAMESPACE_DOCS_H
#define MOLAB_NAMESPACE_DOCS_H

/**
 * @file molab_namespace_docs.h
 * @brief High-level documentation for the project's main namespace.
 */

/**
 * @namespace MoLab
 * @brief Main namespace for the Apogeo simulator core.
 *
 * Groups the main simulation components, plugin management,
 * configuration, logging, time, and output handling.
 *
 * Key components:
 * - `SimulationEngine`: simulation-cycle orchestration.
 * - `PluginManager`: dynamic plugin loading/execution.
 * - `PhysicsIntegrator`: numerical state integration.
 * - `ConfigManager`: global and per-run configuration.
 * - `TimeManager`: simulation time and UTC management.
 * - `OutputManager`: state persistence and export.
 * - `Logger`: core logging system.
 */
namespace MoLab {}

#endif // MOLAB_NAMESPACE_DOCS_H
