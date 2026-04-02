# Plugin de Estructuras - MoLab

## Descripción General

El **Plugin de Estructuras** es un componente fundamental del sistema de simulación MoLab, responsable del análisis estructural dinámico, seguimiento de masa variable y cálculo de momentos de inercia en tiempo real. Este plugin implementa modelos de mecánica estructural que incluyen efectos de carga, vibración, amortiguación y distribución de masa durante el vuelo.

### **Propósito Principal**
- Seguimiento de masa variable por consumo de combustible
- Cálculo de centro de gravedad dinámico
- Análisis de momentos de inercia en tiempo real
- Modelado de efectos estructurales y vibraciones

### **Tipo de Plugin**
- **Clasificación**: Calculador de Física Paralelo (Tipo 1)
- **Ejecución**: Thread-safe, paralela con otros plugins
- **Acceso a Estado**: Solo lectura
- **Salida**: Fuerzas estructurales y torques de inercia

## Modelos Físicos Implementados

###  **Modelo de Masa Variable**

#### **Masa Total del Vehículo**
```
m_total(t) = m_dry + m_fuel(t) + m_payload
```

Donde:
- `m_dry`: Masa seca del vehículo (kg)
- `m_fuel(t)`: Masa de combustible variable (kg)
- `m_payload`: Masa de carga útil (kg)

#### **Consumo de Combustible**
```
dm_fuel/dt = -ṁ_propellant
```

#### **Distribución de Masa**
```
m_fuel(t) = m_fuel_0 * exp(-∫₀ᵗ (ṁ/m_fuel) dτ)
```

### **Centro de Gravedad Dinámico**

#### **Posición del CG**
```
r_cg = (m_dry * r_dry + m_fuel * r_fuel + m_payload * r_payload) / m_total
```

#### **Variación Temporal del CG**
```
dr_cg/dt = (1/m_total) * [dm_fuel/dt * (r_fuel - r_cg)]
```

### **Momentos de Inercia**

#### **Tensor de Inercia Principal**
```
I = [Ixx  Ixy  Ixz]
    [Iyx  Iyy  Iyz]
    [Izx  Izy  Izz]
```

#### **Momentos de Inercia Variables**
```
Ixx(t) = Ixx_dry + Ixx_fuel(t) + Ixx_payload
Iyy(t) = Iyy_dry + Iyy_fuel(t) + Iyy_payload
Izz(t) = Izz_dry + Izz_fuel(t) + Izz_payload
```

#### **Teorema de Steiner**
```
I_total = I_cm + m * d²
```

### **Modelo de Vibraciones**

#### **Frecuencias Naturales**
```
ωn = √(k/m_eff)
```

#### **Amortiguación Estructural**
```
F_damping = -c * v - k * x
```

#### **Respuesta Dinámica**
```
m * ẍ + c * ẋ + k * x = F_external
```

## Parámetros de Configuración

### **Parámetros de Masa**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `dry_mass` | float | 50000.0 | Masa seca del vehículo (kg) |
| `fuel_mass_initial` | float | 500000.0 | Masa inicial de combustible (kg) |
| `payload_mass` | float | 10000.0 | Masa de carga útil (kg) |
| `enable_mass_tracking` | bool | true | Activar seguimiento de masa |
| `fuel_distribution_type` | string | "uniform" | Tipo de distribución de combustible |

### **Parámetros de Geometría**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `vehicle_length` | float | 50.0 | Longitud total del vehículo (m) |
| `vehicle_diameter` | float | 3.7 | Diámetro del vehículo (m) |
| `cg_dry_position` | array | [0,0,25] | Posición CG seco (m) |
| `fuel_tank_center` | array | [0,0,15] | Centro del tanque (m) |
| `fuel_tank_length` | float | 20.0 | Longitud del tanque (m) |

### **Parámetros de Inercia**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `Ixx_dry` | float | 100000.0 | Momento Ixx seco (kg⋅m²) |
| `Iyy_dry` | float | 2000000.0 | Momento Iyy seco (kg⋅m²) |
| `Izz_dry` | float | 2000000.0 | Momento Izz seco (kg⋅m²) |
| `fuel_inertia_factor` | float | 0.8 | Factor de inercia del combustible |
| `enable_inertia_coupling` | bool | true | Activar acoplamiento de inercias |

### **Parámetros Estructurales**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `structural_stiffness` | float | 1e8 | Rigidez estructural (N/m) |
| `damping_coefficient` | float | 1000.0 | Coeficiente de amortiguación |
| `natural_frequency` | float | 5.0 | Frecuencia natural (Hz) |
| `enable_vibration_analysis` | bool | false | Activar análisis de vibraciones |
| `max_load_factor` | float | 4.0 | Factor de carga máximo (g) |

### 📊 **Flujo de Cálculo**

```c
int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    StructuresPlugin* plugin = (StructuresPlugin*)handle;
    
    // 1. Obtener estado actual del vehículo
    Vector3 position = get_position(data->state);
    Vector3 velocity = get_velocity(data->state);
    Vector3 acceleration = get_acceleration(data->state);
    float sim_time = get_simulation_time(data->state);
    
    // 2. Actualizar masa de combustible
    update_fuel_mass(plugin, data, sim_time);
    
    // 3. Calcular centro de gravedad actual
    calculate_center_of_gravity(plugin);
    
    // 4. Actualizar momentos de inercia
    update_inertia_tensor(plugin);
    
    // 5. Análisis de factores de carga
    float load_factor = calculate_load_factor(plugin, acceleration);
    plugin->current_load_factor = load_factor;
    
    // 6. Análisis de vibraciones (si está activado)
    Vector3 vibration_force = {0, 0, 0};
    if (plugin->enable_vibration_analysis) {
        vibration_force = calculate_vibration_effects(plugin, acceleration);
    }
    
    // 7. Fuerzas estructurales
    Vector3 structural_force = calculate_structural_forces(plugin, acceleration);
    
    // 8. Combinar todas las fuerzas estructurales
    Vector3 total_force = vector3_add(structural_force, vibration_force);
    
    // 9. Aplicar límites de seguridad
    if (load_factor > plugin->max_load_factor) {
        LOG_WARNING("Structures: Load factor %.1fg exceeds limit %.1fg",
                   load_factor, plugin->max_load_factor);
    }
    
    // 10. Actualizar salida
    data->output_force->x = total_force.x;
    data->output_force->y = total_force.y;
    data->output_force->z = total_force.z;
    
    // 11. Logging de diagnóstico
    LOG_DEBUG("Structures: Mass=%.0fkg, CG=[%.1f,%.1f,%.1f], LoadFactor=%.1fg",
              plugin->total_mass_current, 
              plugin->cg_current.x, plugin->cg_current.y, plugin->cg_current.z,
              load_factor);
    
    return 0; // Éxito
}
```

### **Actualización de Masa de Combustible**

```c
void update_fuel_mass(StructuresPlugin* plugin, PluginTickData* data, float sim_time) {
    // Obtener flujo másico de propulsión
    float mass_flow_rate = get_propellant_mass_flow(data);
    float delta_time = get_delta_time(data->state);
    
    // Actualizar masa de combustible
    float fuel_consumed = mass_flow_rate * delta_time;
    plugin->fuel_mass_current = fmaxf(0.0f, plugin->fuel_mass_current - fuel_consumed);
    
    // Calcular masa total actual
    plugin->total_mass_current = plugin->dry_mass + 
                                plugin->fuel_mass_current + 
                                plugin->payload_mass;
    
    // Actualizar estado global de masa
    update_vehicle_mass(data->state, plugin->total_mass_current);
}
```

### **Cálculo de Centro de Gravedad**

```c
void calculate_center_of_gravity(StructuresPlugin* plugin) {
    float total_mass = plugin->total_mass_current;
    
    // Contribución de masa seca
    Vector3 cg_contribution_dry = vector3_scale(plugin->cg_dry_position, plugin->dry_mass);
    
    // Contribución de combustible (distribución variable)
    Vector3 fuel_cg = calculate_fuel_center_of_gravity(plugin);
    Vector3 cg_contribution_fuel = vector3_scale(fuel_cg, plugin->fuel_mass_current);
    
    // Contribución de carga útil (asumida en la punta)
    Vector3 payload_position = {0.0f, 0.0f, plugin->vehicle_length * 0.9f};
    Vector3 cg_contribution_payload = vector3_scale(payload_position, plugin->payload_mass);
    
    // Centro de gravedad combinado
    Vector3 cg_total = vector3_add(cg_contribution_dry, 
                       vector3_add(cg_contribution_fuel, cg_contribution_payload));
    
    plugin->cg_current = vector3_scale(cg_total, 1.0f / total_mass);
}
```

### **Actualización de Momentos de Inercia**

```c
void update_inertia_tensor(StructuresPlugin* plugin) {
    // Momentos de inercia de la masa seca (constantes)
    float Ixx = plugin->Ixx_dry;
    float Iyy = plugin->Iyy_dry;
    float Izz = plugin->Izz_dry;
    
    // Contribución del combustible (variable)
    float fuel_fraction = plugin->fuel_mass_current / plugin->fuel_mass_initial;
    float fuel_Ixx = plugin->fuel_inertia_factor * plugin->fuel_mass_current * 
                     powf(plugin->vehicle_diameter / 2.0f, 2);
    float fuel_Iyy = plugin->fuel_inertia_factor * plugin->fuel_mass_current * 
                     (powf(plugin->fuel_tank_length / 2.0f, 2) + 
                      powf(plugin->vehicle_diameter / 4.0f, 2));
    float fuel_Izz = fuel_Iyy; // Simetría axial
    
    // Momentos de inercia totales
    plugin->Ixx_current = Ixx + fuel_Ixx;
    plugin->Iyy_current = Iyy + fuel_Iyy;
    plugin->Izz_current = Izz + fuel_Izz;
    
    // Actualizar tensor de inercia
    plugin->inertia_tensor = create_inertia_matrix(
        plugin->Ixx_current, 
        plugin->Iyy_current, 
        plugin->Izz_current
    );
}
```