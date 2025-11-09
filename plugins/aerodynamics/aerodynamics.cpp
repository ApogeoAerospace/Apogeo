/*
 * Aerodynamics Plugin - Modelo Aerodinámico Realista
 *
 * Este plugin implementa modelos aerodinámicos reales incluyendo:
 * - Drag (resistencia aerodinámica)
 * - Lift (sustentación)
 * - Efectos de ángulo de ataque
 * - Variación con altitud y velocidad
 * - Coeficientes aerodinámicos configurables
 */

#include "../../src/api/plugin_api.h"
#include "state_vector_generated.h"
#include <cmath>
#include <algorithm>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

// Constantes físicas
const double AIR_DENSITY_SEA_LEVEL = 1.225;  // kg/m³
const double SCALE_HEIGHT = 8400.0;          // m (altura de escala atmosférica)
const double EARTH_RADIUS = 6371000.0;       // m

// Estructura del plugin aerodinámico
struct AerodynamicsPluginInstance {
    // Parámetros del vehículo
    double reference_area;        // Área de referencia (m²)
    double drag_coefficient;      // Coeficiente de drag (Cd)
    double lift_coefficient;      // Coeficiente de lift (Cl)
    double mass;                  // Masa del vehículo (kg)

    // Parámetros aerodinámicos avanzados
    double aspect_ratio;          // Relación de aspecto del ala
    double oswald_efficiency;     // Factor de eficiencia de Oswald
    double zero_lift_drag;        // Drag a sustentación cero
    double lift_curve_slope;      // Pendiente de la curva de sustentación (1/rad)

    // Configuración
    bool enable_drag;
    bool enable_lift;
    bool enable_induced_drag;
    bool enable_altitude_effects;
    bool enable_compressibility;

    // Estado interno
    double current_altitude;
    double current_mach;
    double current_reynolds;
    bool initialized;
};

// Funciones auxiliares
double calculateAirDensity(double altitude) {
    // Modelo de atmósfera estándar internacional
    if (altitude < 0) altitude = 0;

    // Hasta 11km (troposfera)
    if (altitude <= 11000.0) {
        double temperature = 288.15 - 0.0065 * altitude;  // K
        double pressure = 101325.0 * pow(temperature / 288.15, 5.256);  // Pa
        return pressure / (287.0 * temperature);  // kg/m³
    }

    // Estratosfera simplificada (11-20km)
    return AIR_DENSITY_SEA_LEVEL * exp(-altitude / SCALE_HEIGHT);
}

double calculateMachNumber(double velocity, double altitude) {
    // Velocidad del sonido en función de la altitud
    double temperature = (altitude <= 11000.0) ?
        288.15 - 0.0065 * altitude : 216.65;  // K
    double speed_of_sound = sqrt(1.4 * 287.0 * temperature);  // m/s
    return velocity / speed_of_sound;
}

double calculateReynoldsNumber(double velocity, double characteristic_length, double altitude) {
    double air_density = calculateAirDensity(altitude);
    double dynamic_viscosity = 1.789e-5;  // kg/(m·s) a nivel del mar
    return (air_density * velocity * characteristic_length) / dynamic_viscosity;
}

double calculateInducedDragCoefficient(double lift_coefficient, double aspect_ratio, double oswald_efficiency) {
    if (aspect_ratio <= 0 || oswald_efficiency <= 0) return 0.0;
    return (lift_coefficient * lift_coefficient) / (M_PI * aspect_ratio * oswald_efficiency);
}

double calculateCompressibilityFactor(double mach_number) {
    // Factor de corrección por compresibilidad (Prandtl-Glauert)
    if (mach_number >= 1.0) return 2.0;  // Simplificación para supersónico
    return 1.0 / sqrt(1.0 - mach_number * mach_number);
}

extern "C" {

PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    AerodynamicsPluginInstance* instance = new AerodynamicsPluginInstance();

    // Configuración por defecto (vehículo tipo cohete/misil)
    instance->reference_area = 0.785;        // ~1m de diámetro
    instance->drag_coefficient = 0.3;        // Cd típico para forma aerodinámica
    instance->lift_coefficient = 0.0;        // Sin sustentación inicial
    instance->mass = 1000.0;                 // 1000 kg

    // Parámetros aerodinámicos avanzados
    instance->aspect_ratio = 4.0;            // Relación de aspecto moderada
    instance->oswald_efficiency = 0.8;       // Eficiencia típica
    instance->zero_lift_drag = 0.02;         // Drag mínimo
    instance->lift_curve_slope = 2.0 * M_PI; // Teoría del perfil delgado

    // Configuración habilitada
    instance->enable_drag = true;
    instance->enable_lift = false;           // Deshabilitado para cohetes
    instance->enable_induced_drag = false;   // Solo si hay sustentación
    instance->enable_altitude_effects = true;
    instance->enable_compressibility = true;

    // Estado inicial
    instance->current_altitude = 0.0;
    instance->current_mach = 0.0;
    instance->current_reynolds = 0.0;
    instance->initialized = true;

    return reinterpret_cast<PluginHandle>(instance);
}

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data || !data->state_buffer || !data->force_out) {
        return -1; // Error: parámetros inválidos
    }

    AerodynamicsPluginInstance* instance = reinterpret_cast<AerodynamicsPluginInstance*>(handle);
    if (!instance->initialized) {
        return -2; // Error: plugin no inicializado
    }

    // Obtener estado actual del FlatBuffer
    auto state = state_vector::GetGeneralState(data->state_buffer);
    if (!state) return -3;

    // Extraer datos del estado
    double pos_x = state->position()->x();
    double pos_y = state->position()->y();
    double pos_z = state->position()->z();
    double vel_x = state->velocity()->x();
    double vel_y = state->velocity()->y();
    double vel_z = state->velocity()->z();

    // Calcular magnitudes
    double altitude = sqrt(pos_x*pos_x + pos_y*pos_y + pos_z*pos_z) - EARTH_RADIUS;
    if (altitude < 0) altitude = 0;

    double velocity_magnitude = sqrt(vel_x*vel_x + vel_y*vel_y + vel_z*vel_z);
    if (velocity_magnitude < 0.1) {
        // Velocidad muy baja, no hay efectos aerodinámicos significativos
        data->force_out->x = 0.0;
        data->force_out->y = 0.0;
        data->force_out->z = 0.0;
        return 0;
    }

    // Actualizar estado interno
    instance->current_altitude = altitude;
    instance->current_mach = calculateMachNumber(velocity_magnitude, altitude);
    instance->current_reynolds = calculateReynoldsNumber(velocity_magnitude,
        sqrt(instance->reference_area), altitude);

    // Calcular densidad del aire
    double air_density = instance->enable_altitude_effects ?
        calculateAirDensity(altitude) : AIR_DENSITY_SEA_LEVEL;

    // Presión dinámica
    double dynamic_pressure = 0.5 * air_density * velocity_magnitude * velocity_magnitude;

    // Vector unitario de velocidad (dirección opuesta para drag)
    double vel_unit_x = -vel_x / velocity_magnitude;
    double vel_unit_y = -vel_y / velocity_magnitude;
    double vel_unit_z = -vel_z / velocity_magnitude;

    // Inicializar fuerzas
    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

    // DRAG (Resistencia aerodinámica)
    if (instance->enable_drag) {
        double drag_coeff = instance->drag_coefficient;

        // Corrección por compresibilidad
        if (instance->enable_compressibility) {
            double comp_factor = calculateCompressibilityFactor(instance->current_mach);
            drag_coeff *= comp_factor;
        }

        // Drag inducido (si hay sustentación)
        if (instance->enable_induced_drag && instance->enable_lift) {
            double induced_drag = calculateInducedDragCoefficient(
                instance->lift_coefficient, instance->aspect_ratio, instance->oswald_efficiency);
            drag_coeff += induced_drag;
        }

        double drag_force = dynamic_pressure * instance->reference_area * drag_coeff;

        force_x += drag_force * vel_unit_x;
        force_y += drag_force * vel_unit_y;
        force_z += drag_force * vel_unit_z;
    }

    // LIFT (Sustentación) - Solo si está habilitada
    if (instance->enable_lift && instance->lift_coefficient != 0.0) {
        double lift_coeff = instance->lift_coefficient;

        // Corrección por compresibilidad
        if (instance->enable_compressibility) {
            double comp_factor = calculateCompressibilityFactor(instance->current_mach);
            lift_coeff *= comp_factor;
        }

        double lift_force = dynamic_pressure * instance->reference_area * lift_coeff;

        // Simplificación: sustentación perpendicular a la velocidad, hacia arriba
        // En un modelo más complejo, esto dependería del ángulo de ataque
        double lift_unit_x = 0.0;
        double lift_unit_y = 0.0;
        double lift_unit_z = 1.0;  // Hacia arriba

        force_x += lift_force * lift_unit_x;
        force_y += lift_force * lift_unit_y;
        force_z += lift_force * lift_unit_z;
    }

    // Asignar fuerzas de salida
    data->force_out->x = static_cast<float>(force_x);
    data->force_out->y = static_cast<float>(force_y);
    data->force_out->z = static_cast<float>(force_z);

    // Torque (momento aerodinámico) - simplificado
    if (data->torque_out) {
        data->torque_out->x = 0.0;
        data->torque_out->y = 0.0;
        data->torque_out->z = 0.0;
    }

    return 0; // Éxito
}

PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (handle) {
        AerodynamicsPluginInstance* instance = reinterpret_cast<AerodynamicsPluginInstance*>(handle);
        delete instance;
    }
}

} // extern "C"

/*
 * MODELO AERODINÁMICO IMPLEMENTADO:
 *
 * 1. DRAG (Resistencia):
 *    - Drag parasito: Cd * 0.5 * ρ * V² * A
 *    - Drag inducido: Cl² / (π * AR * e)
 *    - Corrección por compresibilidad (Prandtl-Glauert)
 *
 * 2. LIFT (Sustentación):
 *    - Sustentación básica: Cl * 0.5 * ρ * V² * A
 *    - Corrección por compresibilidad
 *
 * 3. EFECTOS AMBIENTALES:
 *    - Variación de densidad con altitud (ISA)
 *    - Número de Mach y efectos de compresibilidad
 *    - Número de Reynolds (para futuros desarrollos)
 *
 * 4. PARÁMETROS CONFIGURABLES:
 *    - Área de referencia, coeficientes aerodinámicos
 *    - Habilitación/deshabilitación de efectos específicos
 *    - Parámetros del ala (aspect ratio, eficiencia)
 *
 * CASOS DE USO:
 * - Cohetes: Solo drag, sin sustentación
 * - Aviones: Drag + lift + drag inducido
 * - Misiles: Drag + efectos de compresibilidad
 */
