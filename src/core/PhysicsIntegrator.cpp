#include "PhysicsIntegrator.h"
#include "Logger.h"
#include "state_vector_generated.h"
#include <cmath>
#include <algorithm>

namespace MoLab {

PhysicsIntegrator::PhysicsIntegrator(IntegratorType type)
    : integrator_type_(type), has_previous_state_(false) {
    LOG_INFO("Physics integrator initialized with type: " + getIntegratorTypeName(), "PhysicsIntegrator");
}

PhysicsState PhysicsIntegrator::integrate(const PhysicsState& state,
                                         const Vector3& force,
                                         const Vector3& torque,
                                         double dt) {
    PhysicsState new_state = state;

    // CRÍTICO: Asegurar que el tiempo siempre avance
    new_state.time = state.time + dt;

    switch (integrator_type_) {
        case IntegratorType::EULER:
            new_state = integrateEuler(state, force, torque, dt);
            break;
        case IntegratorType::RUNGE_KUTTA_4:
            new_state = integrateRungeKutta4(state, force, torque, dt);
            break;
        case IntegratorType::VERLET:
            new_state = integrateVerlet(state, force, torque, dt);
            break;
        default:
            LOG_ERROR("Unknown integrator type", "PhysicsIntegrator");
            new_state = state;
            break;
    }

    // Asegurar que el tiempo se actualiza independientemente del método
    new_state.time = state.time + dt;

    // Store for Verlet integration
    previous_state_ = state;
    has_previous_state_ = true;

    return new_state;
}

void PhysicsIntegrator::setIntegratorType(const std::string& type_name) {
    if (type_name == "euler") {
        integrator_type_ = IntegratorType::EULER;
    } else if (type_name == "runge_kutta_4") {
        integrator_type_ = IntegratorType::RUNGE_KUTTA_4;
    } else if (type_name == "verlet") {
        integrator_type_ = IntegratorType::VERLET;
    } else {
        LOG_WARNING("Unknown integrator type: " + type_name + ", using Runge-Kutta 4", "PhysicsIntegrator");
        integrator_type_ = IntegratorType::RUNGE_KUTTA_4;
    }

    LOG_INFO("Integrator type changed to: " + getIntegratorTypeName(), "PhysicsIntegrator");
}

std::string PhysicsIntegrator::getIntegratorTypeName() const {
    switch (integrator_type_) {
        case IntegratorType::EULER: return "euler";
        case IntegratorType::RUNGE_KUTTA_4: return "runge_kutta_4";
        case IntegratorType::VERLET: return "verlet";
        default: return "unknown";
    }
}

PhysicsState PhysicsIntegrator::integrateEuler(const PhysicsState& state,
                                              const Vector3& force,
                                              const Vector3& torque,
                                              double dt) {
    PhysicsState new_state = state;

    // Calcular aceleración: F = ma, por lo tanto a = F/m
    // Protección contra división por cero
    Vector3 acceleration = (state.mass > 1e-10)
        ? force * (1.0 / state.mass)
        : Vector3(0, 0, 0);

    // Integración de Euler para posición y velocidad
    new_state.velocity = state.velocity + acceleration * dt;
    // Usar la nueva velocidad para actualizar posición
    new_state.position = state.position + new_state.velocity * dt;

    // Integración angular (simplificada)
    Vector3 angular_acceleration = (state.mass > 1e-10)
        ? torque * (1.0 / state.mass)
        : Vector3(0, 0, 0);
    new_state.angular_velocity = state.angular_velocity + angular_acceleration * dt;
    new_state.orientation = state.orientation + state.angular_velocity * dt;

    // Actualizar tiempo
    new_state.time = state.time + dt;

    return new_state;
}

PhysicsState PhysicsIntegrator::integrateRungeKutta4(const PhysicsState& state,
                                                    const Vector3& force,
                                                    const Vector3& torque,
                                                    double dt) {
    // RK4 integration
    StateDerivative k1 = calculateDerivative(state, force, torque);

    PhysicsState temp_state = state;
    temp_state.position = state.position + k1.velocity * (dt * 0.5);
    temp_state.velocity = state.velocity + k1.acceleration * (dt * 0.5);
    temp_state.orientation = state.orientation + k1.angular_velocity * (dt * 0.5);
    temp_state.angular_velocity = state.angular_velocity + k1.angular_acceleration * (dt * 0.5);
    StateDerivative k2 = calculateDerivative(temp_state, force, torque);

    temp_state.position = state.position + k2.velocity * (dt * 0.5);
    temp_state.velocity = state.velocity + k2.acceleration * (dt * 0.5);
    temp_state.orientation = state.orientation + k2.angular_velocity * (dt * 0.5);
    temp_state.angular_velocity = state.angular_velocity + k2.angular_acceleration * (dt * 0.5);
    StateDerivative k3 = calculateDerivative(temp_state, force, torque);

    temp_state.position = state.position + k3.velocity * dt;
    temp_state.velocity = state.velocity + k3.acceleration * dt;
    temp_state.orientation = state.orientation + k3.angular_velocity * dt;
    temp_state.angular_velocity = state.angular_velocity + k3.angular_acceleration * dt;
    StateDerivative k4 = calculateDerivative(temp_state, force, torque);

    PhysicsState new_state = state;
    new_state.position = state.position + (k1.velocity + k2.velocity * 2.0 + k3.velocity * 2.0 + k4.velocity) * (dt / 6.0);
    new_state.velocity = state.velocity + (k1.acceleration + k2.acceleration * 2.0 + k3.acceleration * 2.0 + k4.acceleration) * (dt / 6.0);
    new_state.orientation = state.orientation + (k1.angular_velocity + k2.angular_velocity * 2.0 + k3.angular_velocity * 2.0 + k4.angular_velocity) * (dt / 6.0);
    new_state.angular_velocity = state.angular_velocity + (k1.angular_acceleration + k2.angular_acceleration * 2.0 + k3.angular_acceleration * 2.0 + k4.angular_acceleration) * (dt / 6.0);

    return new_state;
}

PhysicsState PhysicsIntegrator::integrateVerlet(const PhysicsState& state,
                                               const Vector3& force,
                                               const Vector3& torque,
                                               double dt) {
    PhysicsState new_state = state;
    // Protección contra división por cero
    Vector3 acceleration = (state.mass > 1e-10)
        ? force * (1.0 / state.mass)
        : Vector3(0, 0, 0);

    if (!has_previous_state_) {
        // First step, use Euler
        new_state.position = state.position + state.velocity * dt + acceleration * (0.5 * dt * dt);
        new_state.velocity = state.velocity + acceleration * dt;
    } else {
        // Verlet integration
        new_state.position = state.position * 2.0 - previous_state_.position + acceleration * (dt * dt);
        // Protección contra división por cero en dt
        new_state.velocity = (dt > 1e-10)
            ? (new_state.position - previous_state_.position) * (1.0 / (2.0 * dt))
            : state.velocity;
    }

    // Simple angular integration for Verlet
    Vector3 angular_acceleration = (state.mass > 1e-10)
        ? torque * (1.0 / state.mass)
        : Vector3(0, 0, 0);
    new_state.orientation = state.orientation + state.angular_velocity * dt + angular_acceleration * (0.5 * dt * dt);
    new_state.angular_velocity = state.angular_velocity + angular_acceleration * dt;

    return new_state;
}

StateDerivative PhysicsIntegrator::calculateDerivative(const PhysicsState& state,
                                                      const Vector3& force,
                                                      const Vector3& torque) {
    StateDerivative derivative;

    derivative.velocity = state.velocity;
    // Protección contra división por cero
    derivative.acceleration = (state.mass > 1e-10)
        ? force * (1.0 / state.mass)
        : Vector3(0, 0, 0);
    derivative.angular_velocity = state.angular_velocity;
    derivative.angular_acceleration = (state.mass > 1e-10)
        ? torque * (1.0 / state.mass)
        : Vector3(0, 0, 0);

    return derivative;
}

PhysicsState PhysicsIntegrator::fromFlatBuffer(const state_vector::GeneralState* fb_state) {
    PhysicsState state;

    if (fb_state->position()) {
        state.position = Vector3(fb_state->position()->x(),
                                fb_state->position()->y(),
                                fb_state->position()->z());
    }

    if (fb_state->velocity()) {
        state.velocity = Vector3(fb_state->velocity()->x(),
                                fb_state->velocity()->y(),
                                fb_state->velocity()->z());
    }

    if (fb_state->orientation()) {
        // Convert quaternion to euler angles (simplified)
        state.orientation = Vector3(fb_state->orientation()->x(),
                                   fb_state->orientation()->y(),
                                   fb_state->orientation()->z());
    }

    // Default mass if not specified
    state.mass = 1000.0; // kg
    state.time = fb_state->Time(); // Note: capital T

    return state;
}

void PhysicsIntegrator::toFlatBuffer(flatbuffers::FlatBufferBuilder& builder,
                                    const PhysicsState& state) {
    // Create Vec3 structs
    auto position = state_vector::Vec3(state.position.x, state.position.y, state.position.z);
    auto velocity = state_vector::Vec3(state.velocity.x, state.velocity.y, state.velocity.z);

    // Create quaternion from euler angles (simplified)
    auto orientation = state_vector::Quaternion(state.orientation.x, state.orientation.y, state.orientation.z, 1.0f);

    // Create gravity vector (default Earth gravity)
    auto gravity = state_vector::Vec3(0.0f, 0.0f, -9.81f);

    // Create the GeneralState
    auto general_state = state_vector::CreateGeneralState(builder,
        &position,
        &velocity,
        &orientation,
        1.225f,  // atm_density (sea level)
        101325.0f, // atm_pressure (sea level)
        288.15f,   // atm_temperature (sea level)
        &gravity,
        static_cast<float>(state.time),
        static_cast<float>(state.time)
    );

    builder.Finish(general_state);
}

// Atmospheric Effects Implementation
namespace AtmosphericEffects {

Vector3 calculateDrag(const Vector3& velocity, double air_density,
                     double drag_coefficient, double reference_area) {
    double speed = velocity.magnitude();
    if (speed < 1e-6) return Vector3(); // No drag if not moving

    Vector3 drag_direction = velocity.normalized() * -1.0; // Opposite to velocity
    double drag_magnitude = 0.5 * air_density * speed * speed * drag_coefficient * reference_area;

    return drag_direction * drag_magnitude;
}

Vector3 calculateWind(const Vector3& wind_velocity, const Vector3& object_velocity,
                     double air_density, double reference_area) {
    Vector3 relative_velocity = wind_velocity - object_velocity;
    double drag_coefficient = 0.47; // Sphere approximation

    return calculateDrag(relative_velocity * -1.0, air_density, drag_coefficient, reference_area);
}

double calculateAirDensity(double altitude, double temperature) {
    // CORREGIDO: Modelo atmosférico ISA (International Standard Atmosphere)
    // Basado en US Standard Atmosphere 1976

    if (altitude < 0) altitude = 0;  // No hay altitudes negativas

    const double R = 287.05;  // J/(kg·K) - Constante del gas para aire

    // TROPOSFERA (0 - 11 km)
    if (altitude <= 11000.0) {
        const double T0 = 288.15;           // K - Temperatura a nivel del mar
        const double P0 = 101325.0;         // Pa - Presión a nivel del mar
        const double L = 0.0065;            // K/m - Gradiente térmico (lapse rate)
        const double g = 9.80665;           // m/s² - Gravedad estándar

        double T = T0 - L * altitude;
        double P = P0 * pow(T / T0, g / (R * L));
        return P / (R * T);  // Ecuación de estado: ρ = P/(RT)
    }

    // ESTRATOSFERA BAJA (11 km - 25 km) - Isotérmica
    else if (altitude <= 25000.0) {
        const double T11 = 216.65;          // K - Temperatura a 11 km
        const double P11 = 22632.1;         // Pa - Presión a 11 km
        const double g = 9.80665;

        double h = altitude - 11000.0;
        double P = P11 * exp(-g * h / (R * T11));
        return P / (R * T11);
    }

    // ESTRATOSFERA MEDIA (25 km - 47 km)
    else if (altitude <= 47000.0) {
        const double T25 = 216.65;
        const double P25 = 2488.66;
        const double L = -0.003;            // Inversión térmica
        const double g = 9.80665;

        double h = altitude - 25000.0;
        double T = T25 + L * h;
        double P = P25 * pow(T / T25, g / (R * L));
        return P / (R * T);
    }

    // ALTITUDES ALTAS (>47 km) - Modelo exponencial simplificado
    else {
        const double rho_47 = 0.00142;      // kg/m³ a 47 km
        const double H = 6400.0;            // m - Scale height para altitudes altas
        double h = altitude - 47000.0;
        return rho_47 * exp(-h / H);
    }
}

} // namespace AtmosphericEffects

// Gravitational Effects Implementation
namespace GravitationalEffects {

Vector3 calculateEarthGravity(const Vector3& position, double mass) {
    // CORREGIDO: Gravedad realista con ley del cuadrado inverso
    // Constantes físicas (CODATA 2018 / WGS84)
    const double G = 6.67430e-11;        // m³/(kg·s²) - Constante gravitacional
    const double M_EARTH = 5.9722e24;    // kg - Masa de la Tierra
    const double R_EARTH = 6378137.0;    // m - Radio ecuatorial (WGS84)
    const double J2 = 1.08262668e-3;     // Coeficiente J2 (achatamiento terrestre)

    // Distancia al centro de la Tierra
    double r = position.magnitude();

    // Protección contra división por cero o posiciones dentro de la Tierra
    if (r < R_EARTH * 0.1) {
        // Muy cerca del centro, usar gravedad estándar hacia abajo
        return Vector3(0, 0, -9.80665 * mass);
    }

    // GRAVEDAD NEWTONIANA: F = -GMm/r² * r̂
    // La dirección apunta HACIA EL CENTRO (no siempre hacia abajo)
    double g_magnitude = G * M_EARTH / (r * r);
    Vector3 direction = position.normalized() * -1.0;  // Hacia el centro
    Vector3 gravity = direction * (g_magnitude * mass);

    // PERTURBACIÓN J2 (achatamiento terrestre)
    // Importante para órbitas LEO, añade ~0.1-1% de corrección
    if (r > R_EARTH) {
        double z = position.z;
        double r2 = r * r;
        double r_ratio = R_EARTH / r;
        double r_ratio_sq = r_ratio * r_ratio;
        double z_over_r = z / r;
        double z_over_r_sq = z_over_r * z_over_r;

        // Componente radial J2
        double j2_radial = 1.5 * J2 * r_ratio_sq * (1.0 - 5.0 * z_over_r_sq);

        // Componente axial J2 (afecta principalmente componente Z)
        double j2_axial = 1.5 * J2 * r_ratio_sq * (3.0 - 5.0 * z_over_r_sq);

        // Aplicar corrección J2
        gravity = gravity * (1.0 + j2_radial);
        gravity.z += g_magnitude * mass * j2_axial * z_over_r;
    }

    return gravity;
}

Vector3 calculateCentralGravity(const Vector3& position, double central_mass,
                               double gravitational_constant) {
    double distance = position.magnitude();
    if (distance < 1e-6) return Vector3(); // Avoid division by zero

    double force_magnitude = gravitational_constant * central_mass / (distance * distance);
    Vector3 direction = position.normalized() * -1.0; // Toward center

    return direction * force_magnitude;
}

} // namespace GravitationalEffects

} // namespace MoLab
