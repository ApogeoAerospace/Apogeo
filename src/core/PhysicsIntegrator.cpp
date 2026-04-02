#include "PhysicsIntegrator.h"
#include "PhysicsIntegrator.h"
#include "Logger.h"
#include "state_vector_generated.h"

/**
 * @file PhysicsIntegrator.cpp
 * @brief Implementación de integración numérica del estado físico.
 */

namespace odeint = boost::numeric::odeint;

namespace MoLab {

namespace {

constexpr double kMassEpsilon = 1e-12;
constexpr double kQuatNormEpsilon = 1e-15;

inline bool hasValidMass(double mass) {
    return std::isfinite(mass) && mass > kMassEpsilon;
}

inline Quaternion4 normalizedOrIdentity(const Quaternion4& q) {
    if (!q.coeffs().allFinite()) {
        return Quaternion4::Identity();
    }

    const double norm = q.norm();
    if (norm < kQuatNormEpsilon) {
        return Quaternion4::Identity();
    }

    Quaternion4 normalized = q;
    normalized.normalize();
    return normalized;
}

} // namespace
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
    dydt.fill(0.0);

    // Extraer variables dinámicas del vector plano
    const Vector3 vel   = odeVelocity(y);
    const Vector3 omega = odeAngularVelocity(y);
    const Quaternion4 q = normalizedOrIdentity(odeOrientation(y));

    // --- Cinemática lineal: x_dot = v ---
    dydt[0] = vel.x();
    dydt[1] = vel.y();
    dydt[2] = vel.z();

    // --- Segunda ley de Newton: v_dot = F / m ---
    if (hasValidMass(mass)) {
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

    Vector3 alpha = Vector3::Zero();
    if (inertia.allFinite() && net_torque.allFinite()) {
        Eigen::LDLT<InertiaTensor3> ldlt(inertia);
        if (ldlt.info() == Eigen::Success) {
            alpha = ldlt.solve(net_torque);
            if (ldlt.info() != Eigen::Success || !alpha.allFinite()) {
                alpha = Vector3::Zero();
            }
        }
    }
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
    if (!std::isfinite(dt)) {
        LOG_WARNING("Invalid time step (NaN/Inf), skipping integration step", "PhysicsIntegrator");
        return state;
    }
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
            // Velocity-Verlet para traslación + RK4 para rotación.
            // Odeint no ofrece velocity_verlet para sistemas mixtos de
            // primer y segundo orden, por lo que se implementa manualmente.

            // --- Traslación: Velocity-Verlet (a = F/m) ---
            Vector3 a_n = Vector3::Zero();
            if (hasValidMass(state.mass))
                a_n = force / state.mass;

            Vector3 v_half  = state.velocity + a_n * (0.5 * dt);
            Vector3 new_pos = state.position + v_half * dt;

            // Aceleración constante durante el paso => a_{n+1} = a_n
            Vector3 new_vel = v_half + a_n * (0.5 * dt);

            // --- Rotación: RK4 explícito sobre (q, omega) ---
            OdeState y_rot = state.toOdeState();
            odeint::runge_kutta4<OdeState> rot_stepper;
            rot_stepper.do_step(system, y_rot, state.time, dt);

            // Aplicar traslación Verlet + rotación RK4
            odePackVec3(y, 0, new_pos);
            odePackVec3(y, 3, new_vel);
            y[6]  = y_rot[6];
            y[7]  = y_rot[7];
            y[8]  = y_rot[8];
            y[9]  = y_rot[9];
            y[10] = y_rot[10];
            y[11] = y_rot[11];
            y[12] = y_rot[12];
            break;
        }
        default: {
            LOG_WARNING("Unknown integrator type, falling back to Runge-Kutta 4", "PhysicsIntegrator");
            odeint::runge_kutta4<OdeState> stepper;
            stepper.do_step(system, y, state.time, dt);
            break;
        }
    }

    // Desempaquetar resultado y normalizar cuaternión
    PhysicsState new_state = state;     // conserva masa e inercia
    new_state.fromOdeState(y);
    new_state.time = state.time + dt;
    new_state.orientation = normalizedOrIdentity(new_state.orientation);

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

    if (fb_state == nullptr) {
        LOG_WARNING("Null FlatBuffer state pointer, returning default PhysicsState", "PhysicsIntegrator");
        return PhysicsState{};
    }
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

    state.orientation = normalizedOrIdentity(state.orientation);
    state.mass = std::isfinite(fb_state->total_mass()) ? std::max(0.0, static_cast<double>(fb_state->total_mass())) : 0.0;
    state.time = std::isfinite(fb_state->sim_time()) ? static_cast<double>(fb_state->sim_time()) : 0.0;
    return state;
}

// PluginManager es el responsable de reconstruir el FlatBuffer del estado.

} // namespace MoLab
