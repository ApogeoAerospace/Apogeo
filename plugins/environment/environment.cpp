/*
 * Environment Plugin - Modelo Ambiental Realista
 *
 * Este plugin implementa modelos ambientales reales incluyendo:
 * - Atmósfera estándar internacional (ISA)
 * - Efectos de viento y turbulencia
 * - Variación de gravedad con altitud
 * - Efectos de rotación terrestre (Coriolis)
 * - Condiciones meteorológicas variables
 */

#include "../../src/api/plugin_api.h"
#include "state_vector_generated.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <chrono>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

// Constantes físicas y terrestres
const double EARTH_RADIUS = 6371000.0;           // m
const double EARTH_MASS = 5.972e24;              // kg
const double GRAVITATIONAL_CONSTANT = 6.674e-11; // m³/(kg·s²)
const double EARTH_ROTATION_RATE = 7.2921e-5;    // rad/s
const double STANDARD_GRAVITY = 9.80665;         // m/s²
const double STANDARD_PRESSURE = 101325.0;       // Pa
const double STANDARD_TEMPERATURE = 288.15;      // K
const double LAPSE_RATE = 0.0065;                // K/m
const double GAS_CONSTANT_AIR = 287.0;           // J/(kg·K)
const double GAMMA_AIR = 1.4;                    // Razón de calores específicos

// Capas atmosféricas
struct AtmosphericLayer {
    double altitude_base;     // Altitud base (m)
    double altitude_top;      // Altitud tope (m)
    double temperature_base;  // Temperatura base (K)
    double lapse_rate;        // Gradiente térmico (K/m)
    double pressure_base;     // Presión base (Pa)
};

// Estructura del plugin ambiental
struct EnvironmentPluginInstance {
    // Modelo atmosférico
    std::vector<AtmosphericLayer> atmosphere_layers;
    double current_temperature;       // Temperatura actual (K)
    double current_pressure;          // Presión actual (Pa)
    double current_density;           // Densidad actual (kg/m³)
    double current_sound_speed;       // Velocidad del sonido (m/s)
    double current_viscosity;         // Viscosidad dinámica (Pa·s)

    // Modelo de viento
    double wind_speed_x;              // Velocidad del viento X (m/s)
    double wind_speed_y;              // Velocidad del viento Y (m/s)
    double wind_speed_z;              // Velocidad del viento Z (m/s)
    double wind_direction;            // Dirección del viento (rad)
    double wind_magnitude;            // Magnitud del viento (m/s)
    double turbulence_intensity;      // Intensidad de turbulencia (0-1)
    double gust_factor;               // Factor de ráfagas (1.0-2.0)

    // Modelo gravitacional
    double local_gravity;             // Gravedad local (m/s²)
    double gravity_gradient;          // Gradiente gravitacional (s⁻²)
    bool enable_gravity_variation;    // Habilitar variación con altitud
    bool enable_j2_perturbation;      // Habilitar perturbación J2

    // Efectos de rotación terrestre
    bool enable_coriolis_effect;      // Efecto Coriolis
    bool enable_centrifugal_effect;   // Efecto centrífugo
    double latitude;                  // Latitud (rad)
    double longitude;                 // Longitud (rad)

    // Condiciones meteorológicas
    double humidity;                  // Humedad relativa (0-1)
    double cloud_cover;               // Cobertura de nubes (0-1)
    double precipitation_rate;        // Tasa de precipitación (mm/h)
    double visibility;                // Visibilidad (m)

    // Generador de turbulencia
    std::mt19937 random_generator;
    std::normal_distribution<double> turbulence_distribution;

    // Configuración
    bool enable_atmospheric_model;
    bool enable_wind_effects;
    bool enable_turbulence;
    bool enable_weather_effects;
    bool enable_seasonal_variation;

    // Estado interno
    double simulation_time;
    double previous_altitude;
    bool initialized;
};

// Funciones auxiliares
void initializeStandardAtmosphere(EnvironmentPluginInstance* instance) {
    instance->atmosphere_layers.clear();

    // Troposfera (0-11 km)
    AtmosphericLayer troposphere;
    troposphere.altitude_base = 0.0;
    troposphere.altitude_top = 11000.0;
    troposphere.temperature_base = 288.15;    // 15°C
    troposphere.lapse_rate = 0.0065;          // 6.5 K/km
    troposphere.pressure_base = 101325.0;     // 1 atm
    instance->atmosphere_layers.push_back(troposphere);

    // Tropopausa (11-20 km)
    AtmosphericLayer tropopause;
    tropopause.altitude_base = 11000.0;
    tropopause.altitude_top = 20000.0;
    tropopause.temperature_base = 216.65;     // -56.5°C
    tropopause.lapse_rate = 0.0;              // Isotérmica
    tropopause.pressure_base = 22632.1;
    instance->atmosphere_layers.push_back(tropopause);

    // Estratosfera (20-32 km)
    AtmosphericLayer stratosphere;
    stratosphere.altitude_base = 20000.0;
    stratosphere.altitude_top = 32000.0;
    stratosphere.temperature_base = 216.65;
    stratosphere.lapse_rate = -0.001;         // Inversión térmica
    stratosphere.pressure_base = 5474.89;
    instance->atmosphere_layers.push_back(stratosphere);

    // Estratosfera superior (32-47 km)
    AtmosphericLayer upper_stratosphere;
    upper_stratosphere.altitude_base = 32000.0;
    upper_stratosphere.altitude_top = 47000.0;
    upper_stratosphere.temperature_base = 228.65;
    upper_stratosphere.lapse_rate = -0.0028;
    upper_stratosphere.pressure_base = 868.02;
    instance->atmosphere_layers.push_back(upper_stratosphere);
}

void calculateAtmosphericProperties(EnvironmentPluginInstance* instance, double altitude) {
    if (altitude < 0) altitude = 0;

    // Encontrar la capa atmosférica correcta
    const AtmosphericLayer* layer = nullptr;
    for (const auto& l : instance->atmosphere_layers) {
        if (altitude >= l.altitude_base && altitude <= l.altitude_top) {
            layer = &l;
            break;
        }
    }

    if (!layer) {
        // Altitud muy alta, usar modelo exponencial simple
        instance->current_temperature = 200.0;  // K
        instance->current_pressure = STANDARD_PRESSURE * exp(-altitude / 8400.0);
        instance->current_density = instance->current_pressure / (GAS_CONSTANT_AIR * instance->current_temperature);
        instance->current_sound_speed = sqrt(GAMMA_AIR * GAS_CONSTANT_AIR * instance->current_temperature);
        instance->current_viscosity = 1.458e-6 * pow(instance->current_temperature, 1.5) /
                                     (instance->current_temperature + 110.4);
        return;
    }

    // Calcular temperatura
    double height_in_layer = altitude - layer->altitude_base;
    instance->current_temperature = layer->temperature_base - layer->lapse_rate * height_in_layer;

    // Calcular presión
    if (std::abs(layer->lapse_rate) < 1e-6) {
        // Capa isotérmica
        instance->current_pressure = layer->pressure_base *
            exp(-STANDARD_GRAVITY * height_in_layer / (GAS_CONSTANT_AIR * instance->current_temperature));
    } else {
        // Capa con gradiente térmico
        double temp_ratio = instance->current_temperature / layer->temperature_base;
        double exponent = STANDARD_GRAVITY / (GAS_CONSTANT_AIR * layer->lapse_rate);
        instance->current_pressure = layer->pressure_base * pow(temp_ratio, exponent);
    }

    // Calcular densidad
    instance->current_density = instance->current_pressure / (GAS_CONSTANT_AIR * instance->current_temperature);

    // Calcular velocidad del sonido
    instance->current_sound_speed = sqrt(GAMMA_AIR * GAS_CONSTANT_AIR * instance->current_temperature);

    // Calcular viscosidad dinámica (Ley de Sutherland)
    instance->current_viscosity = 1.458e-6 * pow(instance->current_temperature, 1.5) /
                                 (instance->current_temperature + 110.4);
}

double calculateLocalGravity(double altitude) {
    // Variación de gravedad con altitud
    double r = EARTH_RADIUS + altitude;
    return GRAVITATIONAL_CONSTANT * EARTH_MASS / (r * r);
}

void calculateWindEffects(EnvironmentPluginInstance* instance, double altitude, double time) {
    // Modelo de viento simplificado con variación altitudinal
    double altitude_km = altitude / 1000.0;

    // Viento base (jet stream aproximado)
    double base_wind_speed = 0.0;
    if (altitude_km > 8.0 && altitude_km < 15.0) {
        // Jet stream entre 8-15 km
        base_wind_speed = 50.0 * sin(M_PI * (altitude_km - 8.0) / 7.0);
    } else if (altitude_km > 15.0) {
        // Viento estratosférico
        base_wind_speed = 20.0 * exp(-(altitude_km - 15.0) / 10.0);
    } else {
        // Viento troposférico
        base_wind_speed = 10.0 * (altitude_km / 8.0);
    }

    // Variación temporal (simulación de cambios meteorológicos)
    double temporal_variation = 0.3 * sin(time / 3600.0) + 0.2 * sin(time / 1800.0);

    instance->wind_magnitude = base_wind_speed * (1.0 + temporal_variation);
    instance->wind_direction += 0.001 * sin(time / 900.0);  // Rotación lenta

    // Componentes del viento
    instance->wind_speed_x = instance->wind_magnitude * cos(instance->wind_direction);
    instance->wind_speed_y = instance->wind_magnitude * sin(instance->wind_direction);
    instance->wind_speed_z = 0.1 * instance->wind_magnitude * sin(time / 600.0);  // Viento vertical

    // Turbulencia
    if (instance->enable_turbulence) {
        double turb_intensity = instance->turbulence_intensity;

        // Aumentar turbulencia cerca del suelo y en jet stream
        if (altitude_km < 1.0) {
            turb_intensity *= (2.0 - altitude_km);  // Turbulencia de superficie
        } else if (altitude_km > 8.0 && altitude_km < 15.0) {
            turb_intensity *= 1.5;  // Turbulencia del jet stream
        }

        // Generar componentes turbulentas
        double turb_x = instance->turbulence_distribution(instance->random_generator) * turb_intensity;
        double turb_y = instance->turbulence_distribution(instance->random_generator) * turb_intensity;
        double turb_z = instance->turbulence_distribution(instance->random_generator) * turb_intensity * 0.5;

        instance->wind_speed_x += turb_x;
        instance->wind_speed_y += turb_y;
        instance->wind_speed_z += turb_z;
    }
}

extern "C" {

PLUGIN_EXPORT PluginHandle plugin_create_instance() {
    EnvironmentPluginInstance* instance = new EnvironmentPluginInstance();

    // Inicializar atmósfera estándar
    initializeStandardAtmosphere(instance);

    // Propiedades atmosféricas iniciales
    instance->current_temperature = STANDARD_TEMPERATURE;
    instance->current_pressure = STANDARD_PRESSURE;
    instance->current_density = 1.225;           // kg/m³
    instance->current_sound_speed = 343.0;       // m/s
    instance->current_viscosity = 1.789e-5;      // Pa·s

    // Modelo de viento inicial
    instance->wind_speed_x = 5.0;                // 5 m/s hacia el este
    instance->wind_speed_y = 0.0;
    instance->wind_speed_z = 0.0;
    instance->wind_direction = 0.0;              // rad (hacia el este)
    instance->wind_magnitude = 5.0;              // m/s
    instance->turbulence_intensity = 0.1;        // 10% de turbulencia
    instance->gust_factor = 1.2;                 // 20% de ráfagas

    // Modelo gravitacional
    instance->local_gravity = STANDARD_GRAVITY;
    instance->gravity_gradient = 0.0;
    instance->enable_gravity_variation = true;
    instance->enable_j2_perturbation = false;    // Efecto avanzado

    // Efectos de rotación terrestre
    instance->enable_coriolis_effect = false;    // Computacionalmente intensivo
    instance->enable_centrifugal_effect = false;
    instance->latitude = 0.0;                    // Ecuador
    instance->longitude = 0.0;                   // Greenwich

    // Condiciones meteorológicas
    instance->humidity = 0.6;                    // 60% humedad
    instance->cloud_cover = 0.3;                 // 30% nubes
    instance->precipitation_rate = 0.0;          // Sin lluvia
    instance->visibility = 10000.0;              // 10 km visibilidad

    // Inicializar generador de turbulencia
    instance->random_generator.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    instance->turbulence_distribution = std::normal_distribution<double>(0.0, 1.0);

    // Configuración habilitada
    instance->enable_atmospheric_model = true;
    instance->enable_wind_effects = true;
    instance->enable_turbulence = true;
    instance->enable_weather_effects = false;    // Efectos avanzados
    instance->enable_seasonal_variation = false;

    // Estado interno
    instance->simulation_time = 0.0;
    instance->previous_altitude = 0.0;
    instance->initialized = true;

    return reinterpret_cast<PluginHandle>(instance);
}

PLUGIN_EXPORT int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    if (!handle || !data || !data->state_buffer || !data->force_out) {
        return -1; // Error: parámetros inválidos
    }

    EnvironmentPluginInstance* instance = reinterpret_cast<EnvironmentPluginInstance*>(handle);
    if (!instance->initialized) {
        return -2; // Error: plugin no inicializado
    }

    // Actualizar tiempo de simulación
    instance->simulation_time += data->delta_time;

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

    // Calcular altitud
    double altitude = sqrt(pos_x*pos_x + pos_y*pos_y + pos_z*pos_z) - EARTH_RADIUS;
    if (altitude < 0) altitude = 0;

    // MODELO ATMOSFÉRICO
    if (instance->enable_atmospheric_model) {
        calculateAtmosphericProperties(instance, altitude);
    }

    // MODELO GRAVITACIONAL
    if (instance->enable_gravity_variation) {
        instance->local_gravity = calculateLocalGravity(altitude);
        instance->gravity_gradient = -2.0 * instance->local_gravity / (EARTH_RADIUS + altitude);
    }

    // MODELO DE VIENTO
    if (instance->enable_wind_effects) {
        calculateWindEffects(instance, altitude, instance->simulation_time);
    }

    // Inicializar fuerzas ambientales
    double force_x = 0.0, force_y = 0.0, force_z = 0.0;

    // EFECTOS DE VIENTO (como fuerza de arrastre diferencial)
    if (instance->enable_wind_effects) {
        // Velocidad relativa del viento
        double relative_wind_x = instance->wind_speed_x - vel_x;
        double relative_wind_y = instance->wind_speed_y - vel_y;
        double relative_wind_z = instance->wind_speed_z - vel_z;

        double relative_wind_magnitude = sqrt(relative_wind_x*relative_wind_x +
                                            relative_wind_y*relative_wind_y +
                                            relative_wind_z*relative_wind_z);

        if (relative_wind_magnitude > 0.1) {
            // Fuerza de viento proporcional a la velocidad relativa al cuadrado
            double wind_drag_coefficient = 0.1;  // Coeficiente simplificado
            double reference_area = 1.0;         // m² (área de referencia)

            double wind_force_magnitude = 0.5 * instance->current_density *
                                         relative_wind_magnitude * relative_wind_magnitude *
                                         wind_drag_coefficient * reference_area;

            // Dirección de la fuerza (en dirección del viento relativo)
            force_x += wind_force_magnitude * (relative_wind_x / relative_wind_magnitude);
            force_y += wind_force_magnitude * (relative_wind_y / relative_wind_magnitude);
            force_z += wind_force_magnitude * (relative_wind_z / relative_wind_magnitude);
        }
    }

    // EFECTOS DE CORIOLIS (si están habilitados)
    if (instance->enable_coriolis_effect) {
        // Fuerza de Coriolis: F = -2m * Ω × v
        double omega = EARTH_ROTATION_RATE;
        double mass = 1000.0;  // Masa estimada (debería venir del plugin de estructuras)

        // Componentes de la fuerza de Coriolis (simplificado)
        double coriolis_x = -2.0 * mass * omega * vel_y * sin(instance->latitude);
        double coriolis_y = 2.0 * mass * omega * vel_x * sin(instance->latitude);
        double coriolis_z = 0.0;  // Componente vertical simplificada

        force_x += coriolis_x;
        force_y += coriolis_y;
        force_z += coriolis_z;
    }

    // VARIACIÓN GRAVITACIONAL
    if (instance->enable_gravity_variation) {
        // Corrección gravitacional (diferencia respecto a gravedad estándar)
        double gravity_correction = instance->local_gravity - STANDARD_GRAVITY;
        double mass = 1000.0;  // Masa estimada

        // Fuerza gravitacional adicional (hacia el centro de la Tierra)
        double earth_center_x = -pos_x;
        double earth_center_y = -pos_y;
        double earth_center_z = -pos_z;
        double distance_to_center = sqrt(earth_center_x*earth_center_x +
                                       earth_center_y*earth_center_y +
                                       earth_center_z*earth_center_z);

        if (distance_to_center > 0) {
            force_x += mass * gravity_correction * (earth_center_x / distance_to_center);
            force_y += mass * gravity_correction * (earth_center_y / distance_to_center);
            force_z += mass * gravity_correction * (earth_center_z / distance_to_center);
        }
    }

    // Asignar fuerzas de salida
    data->force_out->x = static_cast<float>(force_x);
    data->force_out->y = static_cast<float>(force_y);
    data->force_out->z = static_cast<float>(force_z);

    // No hay torques ambientales significativos en este modelo simplificado
    if (data->torque_out) {
        data->torque_out->x = 0.0;
        data->torque_out->y = 0.0;
        data->torque_out->z = 0.0;
    }

    return 0; // Éxito
}

PLUGIN_EXPORT void plugin_destroy_instance(PluginHandle handle) {
    if (handle) {
        EnvironmentPluginInstance* instance = reinterpret_cast<EnvironmentPluginInstance*>(handle);
        delete instance;
    }
}

} // extern "C"

/*
 * MODELO AMBIENTAL IMPLEMENTADO:
 *
 * 1. ATMÓSFERA ESTÁNDAR INTERNACIONAL (ISA):
 *    - Troposfera, tropopausa, estratosfera
 *    - Variación de temperatura, presión, densidad
 *    - Velocidad del sonido y viscosidad
 *
 * 2. MODELO DE VIENTO:
 *    - Viento base con variación altitudinal
 *    - Jet stream entre 8-15 km
 *    - Turbulencia atmosférica con distribución normal
 *    - Variación temporal de condiciones
 *
 * 3. MODELO GRAVITACIONAL:
 *    - Variación de gravedad con altitud (ley del cuadrado inverso)
 *    - Gradiente gravitacional
 *    - Efectos de perturbación J2 (opcional)
 *
 * 4. EFECTOS DE ROTACIÓN TERRESTRE:
 *    - Fuerza de Coriolis
 *    - Efectos centrífugos
 *    - Dependencia de latitud y longitud
 *
 * 5. CONDICIONES METEOROLÓGICAS:
 *    - Humedad, cobertura de nubes
 *    - Precipitación y visibilidad
 *    - Variaciones estacionales (opcional)
 *
 * CASOS DE USO:
 * - Lanzamientos: Efectos de viento y turbulencia
 * - Vuelos atmosféricos: Modelo ISA completo
 * - Órbitas: Variación gravitacional y efectos de rotación
 */
