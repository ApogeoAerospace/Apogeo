#include "PhysicsIntegrator.h"
#include "Logger.h"
#include "state_vector_generated.h"

namespace MoLab {

PhysicsIntegrator::PhysicsIntegrator(IntegratorType type)
    : integrator_type_(type),
      has_previous_state_(false) {
    LOG_INFO("Physics integrator initialized with type: " + getIntegratorTypeName(), "PhysicsIntegrator");
}

PhysicsState PhysicsIntegrator::integrate(const PhysicsState& state,
                                          const Vector3& force,
                                          const Vector3& torque,
                                          double dt) {
    // TODO:
    // - Integrar estado dinámico usando el método seleccionado (Euler, RK4, Verlet).
    // - Usar dt proveniente del buffer general (autoridad del orquestador).
    // - Integrar posición y velocidad lineal con fuerzas explícitas.
    // - Integrar orientación y velocidad angular con torques explícitos.
    // - Evitar cualquier lógica de ambiente/validaciones aquí (solo integración numérica).
    // - Mantener consistencia y estabilidad numérica.

    PhysicsState new_state = state;

    // Avance de tiempo: responsabilidad del integrador (dt viene del orquestador/buffer)
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

    new_state.time = state.time + dt;

    // TODO: Almacenar estado previo para Verlet si se implementa.
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
    // TODO (Euler explícito: integrar lineal y angular con M/I)
    PhysicsState new_state = state;
    new_state.time = state.time + dt;
    return new_state;
}

PhysicsState PhysicsIntegrator::integrateRungeKutta4(const PhysicsState& state,
                                                     const Vector3& force,
                                                     const Vector3& torque,
                                                     double dt) {
    // TODO (RK4: definir derivadas y acumular)
    PhysicsState new_state = state;
    new_state.time = state.time + dt;
    return new_state;
}

PhysicsState PhysicsIntegrator::integrateVerlet(const PhysicsState& state,
                                                const Vector3& force,
                                                const Vector3& torque,
                                                double dt) {
    // TODO (Verlet/Velocity-Verlet: lineal y angular)
    PhysicsState new_state = state;
    new_state.time = state.time + dt;
    return new_state;
}

StateDerivative PhysicsIntegrator::calculateDerivative(const PhysicsState& state,
                                                       const Vector3& force,
                                                       const Vector3& torque) {
    // TODO (derivadas: v_dot, x_dot, w_dot, q_dot)
    StateDerivative derivative;
    derivative.velocity = state.velocity;
    derivative.acceleration = Vector3(0, 0, 0);
    derivative.angular_velocity = state.angular_velocity;
    derivative.angular_acceleration = Vector3(0, 0, 0);
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
        // TODO: Convertir cuaternión a representación interna (si aplica)
        state.orientation = Vector3(fb_state->orientation()->x(),
                                    fb_state->orientation()->y(),
                                    fb_state->orientation()->z());
    }

    if (fb_state->angular_velocity()) {
        state.angular_velocity = Vector3(fb_state->angular_velocity()->x(),
                                         fb_state->angular_velocity()->y(),
                                         fb_state->angular_velocity()->z());
    }

    // TODO: Leer masa total e inercia si procede
    state.mass = 1000.0; // Placeholder

    state.time = fb_state->sim_time();
    return state;
}

// Eliminado: toFlatBuffer. El integrador no debe escribir el buffer.
// PluginManager es el responsable de reconstruir el FlatBuffer del estado.

} // namespace MoLab
