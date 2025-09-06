# Plugin de Aerodinámica - MoLab

## Descripción General

El **Plugin de Aerodinámica** es uno de los componentes más críticos del sistema de simulación MoLab, responsable del cálculo preciso de todas las fuerzas aerodinámicas que actúan sobre el vehículo durante el vuelo. Este plugin implementa modelos físicos avanzados que incluyen efectos de compresibilidad, transiciones de régimen de vuelo y variaciones atmosféricas.

### **Propósito Principal**
- Cálculo de fuerzas de arrastre (drag) y sustentación (lift)
- Modelado de efectos aerodinámicos complejos
- Simulación de transiciones de régimen subsónico a supersónico
- Integración con modelos atmosféricos realistas

### **Tipo de Plugin**
- **Clasificación**: Calculador de Física Paralelo (Tipo 1)
- **Ejecución**: Thread-safe, paralela con otros plugins
- **Acceso a Estado**: Solo lectura
- **Salida**: Fuerzas y torques aerodinámicos

## Modelos Físicos Implementados

### **Modelo de Arrastre (Drag)**

#### **Arrastre Subsónico (Mach < 0.8)**
```
F_drag = 0.5 * ρ * V² * C_d * A_ref
```

Donde:
- `ρ`: Densidad del aire (kg/m³)
- `V`: Velocidad relativa del vehículo (m/s)
- `C_d`: Coeficiente de arrastre
- `A_ref`: Área de referencia (m²)

#### **Arrastre Transónico (0.8 ≤ Mach ≤ 1.2)**
```
C_d_trans = C_d_sub + (C_d_sup - C_d_sub) * f_trans(Mach)
```

Función de transición suave para evitar discontinuidades numéricas.

#### **Arrastre Supersónico (Mach > 1.2)**
```
C_d_sup = C_d_base + C_d_wave(Mach)
```

Incluye efectos de ondas de choque y compresibilidad.

### ⬆**Modelo de Sustentación (Lift)**

#### **Sustentación Básica**
```
F_lift = 0.5 * ρ * V² * C_l * A_ref
```

#### **Efectos de Ángulo de Ataque**
```
C_l = C_l_alpha * α + C_l_0
```

Donde:
- `α`: Ángulo de ataque (radianes)
- `C_l_alpha`: Pendiente de la curva de sustentación
- `C_l_0`: Sustentación a ángulo cero

### 🌡️ **Modelo Atmosférico ISA**

#### **Variación de Densidad con Altitud**
```
ρ(h) = ρ_0 * (T(h)/T_0)^((g*M)/(R*L) - 1)
```

#### **Variación de Temperatura**
```
T(h) = T_0 - L * h  (para h < 11,000m)
```

Donde:
- `h`: Altitud (m)
- `L`: Gradiente térmico (6.5 K/km)
- `g`: Aceleración gravitacional
- `M`: Masa molar del aire
- `R`: Constante universal de gases

## Parámetros de Configuración

### **Parámetros Principales**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `drag_coefficient` | float | 0.3 | Coeficiente de arrastre base |
| `lift_coefficient` | float | 0.1 | Coeficiente de sustentación base |
| `reference_area` | float | 10.0 | Área de referencia (m²) |
| `enable_compressibility` | bool | true | Activar efectos de compresibilidad |
| `mach_transition_low` | float | 0.8 | Mach inferior para transición |
| `mach_transition_high` | float | 1.2 | Mach superior para transición |

### **Parámetros Avanzados**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `supersonic_drag_factor` | float | 2.5 | Factor de incremento supersónico |
| `lift_curve_slope` | float | 5.73 | Pendiente curva sustentación (1/rad) |
| `zero_lift_angle` | float | 0.0 | Ángulo de sustentación cero (rad) |
| `max_angle_of_attack` | float | 0.35 | Ángulo máximo antes de pérdida (rad) |
| `atmospheric_model` | string | "ISA" | Modelo atmosférico a usar |
| `enable_ground_effect` | bool | false | Activar efecto suelo |
| `ground_effect_height` | float | 100.0 | Altura límite efecto suelo (m) |

### 📊 **Flujo de Cálculo**

```c
int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    AerodynamicsPlugin* plugin = (AerodynamicsPlugin*)handle;
    
    // 1. Extraer estado del vehículo
    Vector3 position = get_position(data->state);
    Vector3 velocity = get_velocity(data->state);
    
    // 2. Calcular propiedades atmosféricas
    float altitude = position.z;
    AtmosphericProperties atm = calculate_atmosphere(altitude);
    
    // 3. Calcular número de Mach
    float speed = vector3_magnitude(velocity);
    float mach = speed / atm.speed_of_sound;
    
    // 4. Calcular coeficientes aerodinámicos
    float cd = calculate_drag_coefficient(plugin, mach);
    float cl = calculate_lift_coefficient(plugin, angle_of_attack);
    
    // 5. Calcular fuerzas aerodinámicas
    Vector3 drag_force = calculate_drag_force(cd, atm.density, speed, 
                                             plugin->reference_area, velocity);
    Vector3 lift_force = calculate_lift_force(cl, atm.density, speed, 
                                             plugin->reference_area);
    
    // 6. Combinar fuerzas y aplicar
    Vector3 total_force = vector3_add(drag_force, lift_force);
    
    data->output_force->x = total_force.x;
    data->output_force->y = total_force.y;
    data->output_force->z = total_force.z;
    
    return 0; // Éxito
}
```