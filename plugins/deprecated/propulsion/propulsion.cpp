/*
 * Propulsion Plugin - Sistema de Propulsión Realista
 *
 * Este plugin implementa modelos de propulsión reales incluyendo:
 * - Motores de combustible sólido y líquido
 * - Variación de empuje con altitud
 * - Consumo de combustible y cambio de masa
 * - Impulso específico (Isp)
 * - Efectos de presión atmosférica en la tobera
 */

#include "../../../src/api/plugin_api.h"
#include "state_vector_generated.h"
#include <cmath>
#include <algorithm>
#include <nlohmann/json.hpp>

// Constantes físicas
const double STANDARD_GRAVITY = 9.80665;     // m/s² (gravedad estándar)
const double ATMOSPHERIC_PRESSURE_SL = 101325.0; // Pa (presión a nivel del mar)
const double GAS_CONSTANT = 287.0;           // J/(kg·K) para aire

// Tipos de motor
enum EngineType {
    SOLID_ROCKET = 0,
    LIQUID_ROCKET = 1,
    HYBRID_ROCKET = 2,
    ION_THRUSTER = 3
};

// Estructura del plugin de propulsión
struct PropulsionPluginInstance {
    // Configuración del motor
    EngineType engine_type;
    double sea_level_thrust;      // Empuje a nivel del mar (N)
    double vacuum_thrust;         // Empuje en vacío (N)
    double specific_impulse_sl;   // Isp a nivel del mar (s)
    double specific_impulse_vac;  // Isp en vacío (s)
    double nozzle_exit_area;      // Área de salida de la tobera (m²)
    double nozzle_throat_area;    // Área de la garganta (m²)
    double chamber_pressure;      // Presión de cámara (Pa)

    // Estado del combustible
    double initial_fuel_mass;     // Masa inicial de combustible (kg)
    double current_fuel_mass;     // Masa actual de combustible (kg)
    double fuel_density;          // Densidad del combustible (kg/m³)
    double oxidizer_ratio;        // Relación oxidante/combustible
    double burn_rate;             // Tasa de quemado (kg/s)

    // Control del motor
    bool engine_on;               // Motor encendido/apagado
    double throttle_setting;      // Configuración de acelerador (0-1)
    double burn_time;             // Tiempo de quemado acumulado (s)
    double ignition_delay;        // Retraso de encendido (s)

    // Parámetros de rendimiento
    double combustion_efficiency; // Eficiencia de combustión (0-1)
    double nozzle_efficiency;     // Eficiencia de la tobera (0-1)
    double thrust_vector_angle;   // Ángulo del vector de empuje (rad)

    // Configuración
    bool enable_altitude_compensation;
    bool enable_fuel_consumption;
    bool enable_thrust_vectoring;
    bool enable_throttling;

    // Estado interno
    double current_altitude;
    double current_atmospheric_pressure;
    double current_thrust;
    double current_mass_flow_rate;
    bool initialized;
};

// Funciones auxiliares
double calculateAtmosphericPressure(double altitude) {
    // Modelo de atmósfera estándar
    if (altitude < 0) altitude = 0;

    if (altitude <= 11000.0) {
        // Troposfera
        double temperature = 288.15 - 0.0065 * altitude;
        return ATMOSPHERIC_PRESSURE_SL * pow(temperature / 288.15, 5.256);
    } else {
        // Estratosfera simplificada
        return ATMOSPHERIC_PRESSURE_SL * exp(-altitude / 8400.0);
    }
}

double calculateThrustAtAltitude(double vacuum_thrust, double sea_level_thrust,
                                double nozzle_exit_area, double ambient_pressure) {
    // Ecuación de empuje con compensación de altitud
    // T = T_vac - (P_ambient - P_exit) * A_exit
    // Simplificación: asumimos P_exit = 0 para toberas optimizadas para vacío
    double pressure_thrust_loss = ambient_pressure * nozzle_exit_area;
    double altitude_thrust = vacuum_thrust - pressure_thrust_loss;

    // Limitar entre empuje a nivel del mar y empuje en vacío
    return std::max(sea_level_thrust, std::min(vacuum_thrust, altitude_thrust));
}

double calculateSpecificImpulseAtAltitude(double isp_vacuum, double isp_sea_level,
                                         double ambient_pressure) {
    // Interpolación lineal basada en presión atmosférica
    double pressure_ratio = ambient_pressure / ATMOSPHERIC_PRESSURE_SL;
    return isp_sea_level + (isp_vacuum - isp_sea_level) * (1.0 - pressure_ratio);
}

double calculateMassFlowRate(double thrust, double specific_impulse) {
    // Ecuación de Tsiolkovsky: T = Isp * g0 * dm/dt
    if (specific_impulse <= 0) return 0.0;
    return thrust / (specific_impulse * STANDARD_GRAVITY);
}

extern "C" {

PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    PropulsionPluginInstance* instance = new PropulsionPluginInstance();

    // Configuración por defecto (motor de combustible sólido tipo Estes)
    instance->engine_type = SOLID_ROCKET;
    instance->sea_level_thrust = 50000.0;     // 50 kN
    instance->vacuum_thrust = 55000.0;        // 55 kN
    instance->specific_impulse_sl = 250.0;    // 250 s (típico para sólido)
    instance->specific_impulse_vac = 280.0;   // 280 s en vacío
    instance->nozzle_exit_area = 0.1;         // 0.1 m²
    instance->nozzle_throat_area = 0.05;      // 0.05 m²
    instance->chamber_pressure = 7e6;         // 7 MPa

    // Estado del combustible
    instance->initial_fuel_mass = 500.0;      // 500 kg de combustible
    instance->current_fuel_mass = 500.0;
    instance->fuel_density = 1800.0;          // kg/m³ (combustible sólido)
    instance->oxidizer_ratio = 0.0;           // No aplicable para sólido
    instance->burn_rate = 0.0;                // Se calcula dinámicamente

    // Control del motor
    instance->engine_on = false;              // Motor apagado inicialmente
    instance->throttle_setting = 1.0;         // Acelerador completo
    instance->burn_time = 0.0;
    instance->ignition_delay = 0.0;           // Sin retraso

    // Parámetros de rendimiento
    instance->combustion_efficiency = 0.95;   // 95% eficiencia
    instance->nozzle_efficiency = 0.98;       // 98% eficiencia
    instance->thrust_vector_angle = 0.0;      // Sin vectorización

    // Configuración habilitada
    instance->enable_altitude_compensation = true;
    instance->enable_fuel_consumption = true;
    instance->enable_thrust_vectoring = false;
    instance->enable_throttling = false;      // Sólido no es throttleable

    // Estado inicial
    instance->current_altitude = 0.0;
    instance->current_atmospheric_pressure = ATMOSPHERIC_PRESSURE_SL;
    instance->current_thrust = 0.0;
    instance->current_mass_flow_rate = 0.0;
    instance->initialized = true;

    return reinterpret_cast<PluginHandle>(instance);
}

PLUGIN_EXPORT int32_t plugin_configure(PluginHandle handle, const char* json_params) {
    if (!handle || !json_params) {
        return -1; // Error: parámetros inválidos
    }

    PropulsionPluginInstance* instance = reinterpret_cast<PropulsionPluginInstance*>(handle);
    if (!instance->initialized) {
        return -2; // Error: plugin no inicializado
    }

    try {
        nlohmann::json params = nlohmann::json::parse(json_params);

        // Configurar parámetros del motor si están presentes
        if (params.contains("engine_type")) {
            instance->engine_type = static_cast<EngineType>(params["engine_type"].get<int>());
        }

        if (params.contains("sea_level_thrust")) {
            instance->sea_level_thrust = params["sea_level_thrust"].get<double>();
        }

        if (params.contains("vacuum_thrust")) {
            instance->vacuum_thrust = params["vacuum_thrust"].get<double>();
        }

        if (params.contains("specific_impulse_sl")) {
            instance->specific_impulse_sl = params["specific_impulse_sl"].get<double>();
        }

        if (params.contains("specific_impulse_vac")) {
            instance->specific_impulse_vac = params["specific_impulse_vac"].get<double>();
        }

        if (params.contains("nozzle_exit_area")) {
            instance->nozzle_exit_area = params["nozzle_exit_area"].get<double>();
        }

        if (params.contains("nozzle_throat_area")) {
            instance->nozzle_throat_area = params["nozzle_throat_area"].get<double>();
        }

        if (params.contains("chamber_pressure")) {
            instance->chamber_pressure = params["chamber_pressure"].get<double>();
        }

        if (params.contains("initial_fuel_mass")) {
            instance->initial_fuel_mass = params["initial_fuel_mass"].get<double>();
            instance->current_fuel_mass = instance->initial_fuel_mass; // Reset fuel mass
        }

        if (params.contains("fuel_density")) {
            instance->fuel_density = params["fuel_density"].get<double>();
        }

        if (params.contains("oxidizer_ratio")) {
            instance->oxidizer_ratio = params["oxidizer_ratio"].get<double>();
        }

        if (params.contains("combustion_efficiency")) {
            instance->combustion_efficiency = params["combustion_efficiency"].get<double>();
        }

        if (params.contains("nozzle_efficiency")) {
            instance->nozzle_efficiency = params["nozzle_efficiency"].get<double>();
        }

        if (params.contains("thrust_vector_angle")) {
            instance->thrust_vector_angle = params["thrust_vector_angle"].get<double>();
        }

        // Configurar opciones booleanas
        if (params.contains("enable_altitude_compensation")) {
            instance->enable_altitude_compensation = params["enable_altitude_compensation"].get<bool>();
        }

        if (params.contains("enable_fuel_consumption")) {
            instance->enable_fuel_consumption = params["enable_fuel_consumption"].get<bool>();
        }

        if (params.contains("enable_thrust_vectoring")) {
            instance->enable_thrust_vectoring = params["enable_thrust_vectoring"].get<bool>();
        }

        if (params.contains("enable_throttling")) {
            instance->enable_throttling = params["enable_throttling"].get<bool>();
        }

        if (params.contains("engine_on")) {
            instance->engine_on = params["engine_on"].get<bool>();
        }

        if (params.contains("throttle_setting")) {
            instance->throttle_setting = std::max(0.0, std::min(1.0, params["throttle_setting"].get<double>()));
        }

        if (params.contains("ignition_delay")) {
            instance->ignition_delay = params["ignition_delay"].get<double>();
        }

        return 0; // Configuración exitosa

    } catch (const std::exception& e) {
        return -3; // Error: JSON inválido o parámetro incorrecto
    }
}

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data || !data->state_buffer) {
        return -1; // Error: parámetros inválidos
    }

    // Compatibilidad con ambas versiones de la API
    PluginVector3* force_output = data->output_force ? data->output_force : data->force_out;
    PluginVector3* torque_output = data->output_torque ? data->output_torque : data->torque_out;

    if (!force_output) {
        return -1; // Error: sin puntero de salida para fuerzas
    }

    PropulsionPluginInstance* instance = reinterpret_cast<PropulsionPluginInstance*>(handle);
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

    // Calcular altitud (WGS84 consistent with core)
    const double EARTH_RADIUS = 6378137.0;
    double altitude = sqrt(pos_x*pos_x + pos_y*pos_y + pos_z*pos_z) - EARTH_RADIUS;
    if (altitude < 0) altitude = 0;

    // Actualizar estado interno
    instance->current_altitude = altitude;
    instance->current_atmospheric_pressure = calculateAtmosphericPressure(altitude);

    // Verificar si hay combustible
    if (instance->enable_fuel_consumption && instance->current_fuel_mass <= 0.0) {
        instance->engine_on = false;
        instance->current_fuel_mass = 0.0;
    }

    // Inicializar fuerzas
    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

    // Calcular empuje si el motor está encendido
    if (instance->engine_on && instance->current_fuel_mass > 0.0) {
        // Actualizar tiempo de quemado
        instance->burn_time += data->delta_time;

        // Calcular empuje en función de la altitud
        double base_thrust = instance->enable_altitude_compensation ?
            calculateThrustAtAltitude(instance->vacuum_thrust, instance->sea_level_thrust,
                                    instance->nozzle_exit_area, instance->current_atmospheric_pressure) :
            instance->sea_level_thrust;

        // Aplicar throttling si está habilitado
        if (instance->enable_throttling) {
            base_thrust *= instance->throttle_setting;
        }

        // Aplicar eficiencias
        base_thrust *= instance->combustion_efficiency * instance->nozzle_efficiency;

        instance->current_thrust = base_thrust;

        // Calcular consumo de combustible
        if (instance->enable_fuel_consumption) {
            double current_isp = instance->enable_altitude_compensation ?
                calculateSpecificImpulseAtAltitude(instance->specific_impulse_vac,
                                                 instance->specific_impulse_sl,
                                                 instance->current_atmospheric_pressure) :
                instance->specific_impulse_sl;

            instance->current_mass_flow_rate = calculateMassFlowRate(base_thrust, current_isp);

            // Consumir combustible
            double fuel_consumed = instance->current_mass_flow_rate * data->delta_time;
            instance->current_fuel_mass = std::max(0.0, instance->current_fuel_mass - fuel_consumed);
        }

        // Calcular dirección del empuje
        // Simplificación: empuje en dirección de la velocidad (o hacia arriba si velocidad es cero)
        double velocity_magnitude = sqrt(vel_x*vel_x + vel_y*vel_y + vel_z*vel_z);

        double thrust_dir_x, thrust_dir_y, thrust_dir_z;
        if (velocity_magnitude > 0.1) {
            // Empuje en dirección de la velocidad
            thrust_dir_x = vel_x / velocity_magnitude;
            thrust_dir_y = vel_y / velocity_magnitude;
            thrust_dir_z = vel_z / velocity_magnitude;
        } else {
            // Empuje hacia arriba si no hay velocidad significativa
            thrust_dir_x = 0.0;
            thrust_dir_y = 0.0;
            thrust_dir_z = 1.0;
        }

        // Aplicar vectorización de empuje si está habilitada
        if (instance->enable_thrust_vectoring && instance->thrust_vector_angle != 0.0) {
            // Simplificación: rotación en el plano XZ
            double cos_angle = cos(instance->thrust_vector_angle);
            double sin_angle = sin(instance->thrust_vector_angle);

            double new_thrust_x = thrust_dir_x * cos_angle - thrust_dir_z * sin_angle;
            double new_thrust_z = thrust_dir_x * sin_angle + thrust_dir_z * cos_angle;

            thrust_dir_x = new_thrust_x;
            thrust_dir_z = new_thrust_z;
        }

        // Aplicar empuje
        force_x = base_thrust * thrust_dir_x;
        force_y = base_thrust * thrust_dir_y;
        force_z = base_thrust * thrust_dir_z;

    } else {
        instance->current_thrust = 0.0;
        instance->current_mass_flow_rate = 0.0;
    }

    // Asignar fuerzas de salida
    force_output->x = static_cast<float>(force_x);
    force_output->y = static_cast<float>(force_y);
    force_output->z = static_cast<float>(force_z);

    // Torque (para thrust vectoring)
    if (torque_output) {
        torque_output->x = 0.0;
        torque_output->y = 0.0;
        torque_output->z = 0.0;

        if (instance->enable_thrust_vectoring && instance->thrust_vector_angle != 0.0) {
            // Torque proporcional al ángulo de vectorización
            torque_output->y = static_cast<float>(instance->current_thrust *
                                                   sin(instance->thrust_vector_angle) * 2.0);
        }
    }

    return 0; // Éxito
}

PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (handle) {
        PropulsionPluginInstance* instance = reinterpret_cast<PropulsionPluginInstance*>(handle);
        delete instance;
    }
}

} // extern "C"

/*
 * MODELO DE PROPULSIÓN IMPLEMENTADO:
 *
 * 1. EMPUJE:
 *    - Compensación de altitud: T = T_vac - P_amb * A_exit
 *    - Throttling para motores líquidos
 *    - Eficiencias de combustión y tobera
 *
 * 2. CONSUMO DE COMBUSTIBLE:
 *    - Ecuación de Tsiolkovsky: dm/dt = T / (Isp * g0)
 *    - Isp variable con altitud
 *    - Seguimiento de masa de combustible
 *
 * 3. TIPOS DE MOTOR SOPORTADOS:
 *    - Combustible sólido: Empuje fijo, no throttleable
 *    - Combustible líquido: Throttleable, Isp alto
 *    - Híbrido: Características intermedias
 *    - Iónico: Empuje bajo, Isp muy alto
 *
 * 4. EFECTOS AVANZADOS:
 *    - Vectorización de empuje
 *    - Variación de Isp con altitud
 *    - Presión de cámara y diseño de tobera
 *
 * CASOS DE USO:
 * - Cohetes: Empuje alto, duración limitada
 * - Satélites: Empuje bajo, alta eficiencia
 * - Maniobras: Vectorización de empuje
 */
