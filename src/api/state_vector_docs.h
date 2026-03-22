#ifndef STATE_VECTOR_DOCS_H
#define STATE_VECTOR_DOCS_H

/**
 * @file state_vector_docs.h
 * @brief Documentación Doxygen del modelo de estado serializado con FlatBuffers.
 */

/**
 * @defgroup StateVectorModel Modelo de Vector de Estado
 * @brief Modelo de datos de estado de simulación serializado con FlatBuffers.
 *
 * Este grupo documenta los tipos generados desde `src/schemas/state_vector.fbs`
 * y su semántica funcional dentro del núcleo de simulación.
 */

/**
 * @namespace state_vector
 * @brief Tipos FlatBuffers que representan el estado global de simulación.
 *
 * Este namespace es generado a partir del schema `src/schemas/state_vector.fbs`.
 * Contiene estructuras matemáticas básicas y la tabla raíz `GeneralState`.
 *
 * @ingroup StateVectorModel
 */
namespace state_vector {

/**
 * @struct Vec3
 * @brief Vector 3D (x, y, z).
 */
struct Vec3;

/**
 * @struct Quaternion
 * @brief Cuaternión de orientación (x, y, z, w).
 */
struct Quaternion;

/**
 * @struct InertiaTensor
 * @brief Tensor de inercia del vehículo en su marco de referencia.
 */
struct InertiaTensor;

/**
 * @struct EngineCmd
 * @brief Comando por motor: nivel de aceleración y ángulos TVC.
 */
struct EngineCmd;

/**
 * @struct GeneralState
 * @brief Tabla raíz con el estado completo de simulación.
 *
 * Incluye cinemática, dinámica, propiedades aerodinámicas,
 * entorno atmosférico y comandos de actuadores.
 *
 * Campos por sección:
 * - Tiempo e integración: `sim_time`, `dt`
 * - Cinemática: `position`, `velocity`, `orientation`, `angular_velocity`
 * - Dinámica: `total_mass`, `cg_location`, `inertia_tensor`, `propellant_masses`
 * - Aerodinámica: `mach_number`, `dynamic_pressure`, `angle_of_attack`, `sideslip_angle`
 * - Entorno: `atm_density`, `atm_pressure`, `atm_temperature`, `wind_velocity`, `gravity`
 * - Control/actuación: `engines`, `surface_deflections`
 */
struct GeneralState;

} // namespace state_vector

#endif // STATE_VECTOR_DOCS_H
