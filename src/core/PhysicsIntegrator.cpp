#include "PhysicsIntegrator.h"
#include "Logger.h"
#include "state_vector_generated.h"

/**
 * @file PhysicsIntegrator.cpp
 * @brief Implementación de integración numérica del estado físico.
 */

namespace odeint = boost::numeric::odeint;

namespace MoLab {

// ---------------------------------------------------------------------------
// PhysicsState: conversión entre estado estructurado y vector plano
// ---------------------------------------------------------------------------

OdeState PhysicsState::toOdeState() const {
    OdeState y{};
    odePackVec3(y, 0, position);
    odePackVec3(y, 3, velocity);
    odePackQuat(y, orientation);
    odePackVec3(y, 10, angular_velocity);
    return y;
}

void PhysicsState::fromOdeState(const OdeState& y) {
    position         = odePosition(y);
    velocity         = odeVelocity(y);
    orientation      = odeOrientation(y);
    angular_velocity = odeAngularVelocity(y);
}

// ---------------------------------------------------------------------------
// PhysicsIntegrator
// ---------------------------------------------------------------------------

PhysicsIntegrator::PhysicsIntegrator(IntegratorType type)
    : integrator_type_(type) {
    LOG_INFO("Physics integrator initialized with type: " + getIntegratorTypeName(), "PhysicsIntegrator");
}

// ---------------------------------------------------------------------------
// computeOdeDerivatives — núcleo del sistema ODE
// ---------------------------------------------------------------------------
// Opera directamente sobre OdeState, extrayendo solo los Eigen temporales
// necesarios para las operaciones de álgebra lineal (producto cruz,
// producto cuaternión, LDLT solve). Evita reconstruir un PhysicsState
// completo (incluyendo copiar la Matrix3d de inercia) en cada evaluación.

void PhysicsIntegrator::computeOdeDerivatives(const OdeState& y, OdeState& dydt,
                                              double mass,
                                              const InertiaTensor3& inertia,
                                              const Vector3& force,
                                              const Vector3& torque) {
    // Extraer variables dinámicas del vector plano
    const Vector3 vel   = odeVelocity(y);
    const Vector3 omega = odeAngularVelocity(y);
    const Quaternion4 q = odeOrientation(y);

    // --- Cinemática lineal: x_dot = v ---
    dydt[0] = vel.x();
    dydt[1] = vel.y();
    dydt[2] = vel.z();

    // --- Segunda ley de Newton: v_dot = F / m ---
    if (mass > 1e-12) {
        double inv_mass = 1.0 / mass;
        dydt[3] = force.x() * inv_mass;
        dydt[4] = force.y() * inv_mass;
        dydt[5] = force.z() * inv_mass;
    } else {
        dydt[3] = 0.0; dydt[4] = 0.0; dydt[5] = 0.0;
    }

    // --- Cinemática rotacional: q_dot = 0.5 * q * (0, omega) ---
    // Producto de Hamilton entre cuaternión de orientación y cuaternión
    // puro de velocidad angular, escalado por 0.5.
    Quaternion4 omega_quat(0.0, omega.x(), omega.y(), omega.z());
    Quaternion4 q_dot;
    q_dot.coeffs() = (q * omega_quat).coeffs() * 0.5;
    dydt[6] = q_dot.w();
    dydt[7] = q_dot.x();
    dydt[8] = q_dot.y();
    dydt[9] = q_dot.z();

    // --- Ecuaciones de Euler rotacionales: ---
    //   omega_dot = I^(-1) * (tau - omega x (I * omega))
    // Se usa descomposición LDLT (estable para matrices simétricas
    // semidefinidas positivas) en lugar de inversión explícita.
    Vector3 i_omega    = inertia * omega;
    Vector3 gyroscopic = omega.cross(i_omega);
    Vector3 net_torque = torque - gyroscopic;
    Vector3 alpha      = inertia.ldlt().solve(net_torque);
    dydt[10] = alpha.x();
    dydt[11] = alpha.y();
    dydt[12] = alpha.z();
}

// ---------------------------------------------------------------------------
// integrate — despacha al stepper de odeint correspondiente
// ---------------------------------------------------------------------------

PhysicsState PhysicsIntegrator::integrate(const PhysicsState& state,
                                          const Vector3& force,
                                          const Vector3& torque,
                                          double dt) {
    OdeState y = state.toOdeState();

    // Lambda del sistema ODE. Captura por referencia los parámetros constantes
    // del paso (masa, inercia, fuerzas) sin copiarlos.
    auto system = [&](const OdeState& y_in, OdeState& dydt, double /*t*/) {
        computeOdeDerivatives(y_in, dydt, state.mass, state.inertia, force, torque);
    };

    switch (integrator_type_) {
        case IntegratorType::EULER: {
            odeint::euler<OdeState> stepper;
            stepper.do_step(system, y, state.time, dt);
            break;
        }
        case IntegratorType::RUNGE_KUTTA_4: {
            odeint::runge_kutta4<OdeState> stepper;
            stepper.do_step(system, y, state.time, dt);
            break;
        }
        case IntegratorType::VERLET: {
            // Velocity-Verlet para traslación + Euler para rotación.
            // Odeint no ofrece velocity_verlet para sistemas mixtos de
            // primer y segundo orden, por lo que se implementa manualmente.

            // --- Traslación: Velocity-Verlet (a = F/m) ---
            Vector3 a_n = Vector3::Zero();
            if (state.mass > 1e-12)
                a_n = force / state.mass;

            Vector3 v_half  = state.velocity + a_n * (0.5 * dt);
            Vector3 new_pos = state.position + v_half * dt;

            // Aceleración constante durante el paso => a_{n+1} = a_n
            Vector3 new_vel = v_half + a_n * (0.5 * dt);

            // --- Rotación: Euler explícito ---
            OdeState dydt{};
            computeOdeDerivatives(state.toOdeState(), dydt,
                                  state.mass, state.inertia, force, torque);

            // Aplicar derivadas del cuaternión y velocidad angular
            odePackVec3(y, 0, new_pos);
            odePackVec3(y, 3, new_vel);
            y[6]  = state.orientation.w()      + dydt[6]  * dt;
            y[7]  = state.orientation.x()      + dydt[7]  * dt;
            y[8]  = state.orientation.y()      + dydt[8]  * dt;
            y[9]  = state.orientation.z()      + dydt[9]  * dt;
            y[10] = state.angular_velocity.x() + dydt[10] * dt;
            y[11] = state.angular_velocity.y() + dydt[11] * dt;
            y[12] = state.angular_velocity.z() + dydt[12] * dt;
            break;
        }
        default:
            LOG_ERROR("Unknown integrator type", "PhysicsIntegrator");
            break;
    }

    // Desempaquetar resultado y normalizar cuaternión
    PhysicsState new_state = state;     // conserva masa e inercia
    new_state.fromOdeState(y);
    new_state.time = state.time + dt;
    new_state.orientation.normalize();

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

PhysicsState PhysicsIntegrator::fromFlatBuffer(const state_vector::GeneralState* fb_state) {
    // Convierte el estado serializado (FlatBuffer) a PhysicsState interno (double).
    // La conversión float->double ocurre aquí; la truncación inversa es
    // responsabilidad de PluginManager al reconstruir el buffer.

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
        // Constructor de Eigen::Quaterniond: (w, x, y, z)
        state.orientation = Quaternion4(
            fb_state->orientation()->w(),
            fb_state->orientation()->x(),
            fb_state->orientation()->y(),
            fb_state->orientation()->z()
        );
    }

    if (fb_state->angular_velocity()) {
        state.angular_velocity = Vector3(fb_state->angular_velocity()->x(),
                                         fb_state->angular_velocity()->y(),
                                         fb_state->angular_velocity()->z());
    }

    if (fb_state->inertia_tensor()) {
        state.inertia = makeInertiaTensor(
            fb_state->inertia_tensor()->ixx(), fb_state->inertia_tensor()->iyy(),
            fb_state->inertia_tensor()->izz(), fb_state->inertia_tensor()->ixy(),
            fb_state->inertia_tensor()->ixz(), fb_state->inertia_tensor()->iyz());
    }

    state.mass = fb_state->total_mass();
    state.time = fb_state->sim_time();
    return state;
}

// PluginManager es el responsable de reconstruir el FlatBuffer del estado.

} // namespace MoLab
