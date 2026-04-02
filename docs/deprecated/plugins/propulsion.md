# Plugin de Propulsión - MoLab

## Descripción General

El **Plugin de Propulsión** es el corazón del sistema de simulación de vehículos espaciales en MoLab, responsable del modelado preciso de sistemas de propulsión química, eléctrica y híbrida. Este plugin implementa modelos termodinámicos avanzados que incluyen efectos de altitud, perfiles de throttle variables y consumo de combustible realista.

### **Propósito Principal**
- Cálculo de fuerzas de empuje (thrust) variable por altitud
- Modelado de consumo de combustible y flujo másico
- Simulación de perfiles de throttle complejos
- Integración con sistemas de control de vuelo

### **Tipo de Plugin**
- **Clasificación**: Calculador de Física Paralelo (Tipo 1)
- **Ejecución**: Thread-safe, paralela con otros plugins
- **Acceso a Estado**: Solo lectura
- **Salida**: Fuerzas de empuje y torques de control

## Modelos Físicos Implementados

### **Modelo de Empuje (Thrust)**

#### **Empuje a Nivel del Mar**
```
F_thrust = ṁ * v_e + (p_e - p_a) * A_e
```

Donde:
- `ṁ`: Flujo másico de propelente (kg/s)
- `v_e`: Velocidad de escape efectiva (m/s)
- `p_e`: Presión de salida del motor (Pa)
- `p_a`: Presión atmosférica (Pa)
- `A_e`: Área de salida de la tobera (m²)

#### **Empuje en Vacío**
```
F_vacuum = ṁ * v_e + p_e * A_e
```

#### **Compensación por Altitud**
```
F(h) = F_sl + (F_vac - F_sl) * (1 - p(h)/p_0)
```

Donde:
- `F_sl`: Empuje a nivel del mar
- `F_vac`: Empuje en vacío
- `p(h)`: Presión atmosférica a altitud h
- `p_0`: Presión a nivel del mar

### **Modelo de Consumo de Combustible**

#### **Flujo Másico**
```
ṁ = F_thrust / (I_sp * g_0)
```

Donde:
- `I_sp`: Impulso específico (s)
- `g_0`: Aceleración gravitacional estándar (9.80665 m/s²)

#### **Masa Variable**
```
m(t) = m_0 - ∫₀ᵗ ṁ(τ) dτ
```

#### **Ecuación de Tsiolkovsky**
```
Δv = I_sp * g_0 * ln(m_0 / m_f)
```

### **Perfiles de Throttle**

#### **Throttle Constante**
```
T(t) = T_nominal
```

#### **Throttle Gradual**
```
T(t) = T_min + (T_max - T_min) * (t / t_ramp)
```

#### **Throttle por Fases**
```
T(t) = {
  T_ignition,     0 ≤ t < t_1
  T_ascent,       t_1 ≤ t < t_2
  T_cruise,       t_2 ≤ t < t_3
  T_cutoff,       t ≥ t_3
}
```

## Parámetros de Configuración

### **Parámetros Principales**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `sea_level_thrust` | float | 500000.0 | Empuje a nivel del mar (N) |
| `vacuum_thrust` | float | 600000.0 | Empuje en vacío (N) |
| `specific_impulse_sl` | float | 280.0 | Isp a nivel del mar (s) |
| `specific_impulse_vac` | float | 350.0 | Isp en vacío (s) |
| `burn_time` | float | 120.0 | Tiempo de quemado (s) |
| `throttle_profile` | string | "constant" | Perfil de throttle |

### **Parámetros de Control**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `throttle_min` | float | 0.4 | Throttle mínimo (fracción) |
| `throttle_max` | float | 1.0 | Throttle máximo (fracción) |
| `throttle_ramp_time` | float | 5.0 | Tiempo de rampa (s) |
| `enable_throttle_control` | bool | true | Control automático de throttle |
| `thrust_vector_angle` | float | 0.0 | Ángulo de vectorización (rad) |
| `gimbal_range` | float | 0.087 | Rango de gimbal (±5°) |

### **Parámetros Avanzados**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `chamber_pressure` | float | 7000000.0 | Presión de cámara (Pa) |
| `nozzle_expansion_ratio` | float | 40.0 | Relación de expansión |
| `combustion_efficiency` | float | 0.98 | Eficiencia de combustión |
| `nozzle_efficiency` | float | 0.95 | Eficiencia de tobera |
| `propellant_density` | float | 1000.0 | Densidad promedio (kg/m³) |
| `tank_pressure_drop` | float | 100000.0 | Caída de presión (Pa) |

### **Flujo de Cálculo**

```c
int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    PropulsionPlugin* plugin = (PropulsionPlugin*)handle;
    
    // 1. Obtener tiempo de simulación
    float sim_time = get_simulation_time(data->state);
    float burn_time = sim_time - plugin->burn_start_time;
    
    // 2. Determinar estado del motor
    if (burn_time < 0 || burn_time > plugin->burn_duration) {
        plugin->engine_running = false;
        return 0; // Motor apagado
    }
    
    // 3. Calcular throttle actual
    float throttle = calculate_throttle(plugin->throttle_profile, burn_time);
    plugin->current_throttle = throttle;
    
    // 4. Obtener condiciones atmosféricas
    Vector3 position = get_position(data->state);
    float altitude = position.z;
    float atmospheric_pressure = calculate_pressure(altitude);
    
    // 5. Calcular empuje compensado por altitud
    float thrust_compensation = calculate_altitude_compensation(
        plugin->sea_level_thrust, 
        plugin->vacuum_thrust, 
        atmospheric_pressure
    );
    
    float current_thrust = thrust_compensation * throttle;
    
    // 6. Calcular consumo de combustible
    float isp_current = interpolate_isp(plugin, atmospheric_pressure);
    float mass_flow = current_thrust / (isp_current * 9.80665f);
    plugin->current_mass_flow = mass_flow;
    
    // 7. Aplicar vectorización de empuje
    Vector3 thrust_vector = calculate_thrust_vector(
        current_thrust, 
        plugin->gimbal_angle_pitch, 
        plugin->gimbal_angle_yaw
    );
    
    // 8. Actualizar salida
    data->output_force->x = thrust_vector.x;
    data->output_force->y = thrust_vector.y;
    data->output_force->z = thrust_vector.z;
    
    // 9. Logging de diagnóstico
    LOG_DEBUG("Propulsion: T=%.1fs, Throttle=%.2f, Thrust=%.0fN, mdot=%.1fkg/s",
              burn_time, throttle, current_thrust, mass_flow);
    
    return 0; // Éxito
}
```

### **Cálculo de Compensación por Altitud**

```c
float calculate_altitude_compensation(float F_sl, float F_vac, float p_atm) {
    const float p_sea_level = 101325.0f; // Pa
    
    // Factor de compensación lineal
    float compensation_factor = 1.0f - (p_atm / p_sea_level);
    
    // Interpolación entre empuje a nivel del mar y vacío
    return F_sl + (F_vac - F_sl) * compensation_factor;
}
```

### **Evaluación de Perfiles de Throttle**

```c
float calculate_throttle(ThrottleProfile* profile, float burn_time) {
    if (!profile || profile->num_phases == 0) {
        return 1.0f; // Throttle constante
    }
    
    // Buscar fase actual
    for (int i = 0; i < profile->num_phases - 1; i++) {
        if (burn_time >= profile->phases[i].time && 
            burn_time < profile->phases[i + 1].time) {
            
            // Interpolación lineal entre fases
            float t1 = profile->phases[i].time;
            float t2 = profile->phases[i + 1].time;
            float th1 = profile->phases[i].throttle;
            float th2 = profile->phases[i + 1].throttle;
            
            float factor = (burn_time - t1) / (t2 - t1);
            return th1 + (th2 - th1) * factor;
        }
    }
    
    // Usar última fase si estamos más allá del perfil
    return profile->phases[profile->num_phases - 1].throttle;
}
```