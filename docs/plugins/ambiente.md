# Plugin de Ambiente - MoLab

## Descripción General

El **Plugin de Ambiente** modela las condiciones ambientales que afectan al vehículo durante el vuelo, incluyendo atmósfera, viento, turbulencia y efectos gravitacionales variables. Este plugin implementa modelos atmosféricos estándar y efectos ambientales realistas.

### **Propósito Principal**
- Modelado de atmósfera estándar internacional (ISA)
- Simulación de efectos de viento y turbulencia
- Cálculo de variaciones gravitacionales
- Integración con efectos Coriolis y rotación terrestre

### **Tipo de Plugin**
- **Clasificación**: Calculador de Física Paralelo (Tipo 1)
- **Ejecución**: Thread-safe, paralela con otros plugins
- **Acceso a Estado**: Solo lectura
- **Salida**: Fuerzas ambientales y efectos atmosféricos

## Modelos Físicos Implementados

### **Modelo Atmosférico ISA**

#### **Densidad del Aire**
```
ρ(h) = ρ₀ * (T(h)/T₀)^((g*M)/(R*L) - 1)
```

#### **Temperatura**
```
T(h) = T₀ - L * h  (para h < 11,000m)
T(h) = T₁ = 216.65K  (para 11,000m ≤ h < 20,000m)
```

#### **Presión**
```
p(h) = p₀ * (T(h)/T₀)^((g*M)/(R*L))
```

### **Modelo de Viento**

#### **Viento Constante**
```
V_wind = [Vx, Vy, Vz]
```

#### **Perfil de Viento por Altitud**
```
V_wind(h) = V₀ * (h/h₀)^α
```

#### **Turbulencia**
```
V_turb(t) = A * sin(2π * f * t + φ)
```

### **Efectos Gravitacionales**

#### **Variación con Altitud**
```
g(h) = g₀ * (R_earth / (R_earth + h))²
```

#### **Efectos Coriolis**
```
F_coriolis = -2 * m * Ω × v
```

## Parámetros de Configuración

### **Parámetros Atmosféricos**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `atmospheric_model` | string | "ISA" | Modelo atmosférico |
| `sea_level_pressure` | float | 101325.0 | Presión nivel del mar (Pa) |
| `sea_level_temperature` | float | 288.15 | Temperatura nivel del mar (K) |
| `sea_level_density` | float | 1.225 | Densidad nivel del mar (kg/m³) |
| `temperature_lapse_rate` | float | 0.0065 | Gradiente térmico (K/m) |

### **Parámetros de Viento**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `enable_wind` | bool | false | Activar efectos de viento |
| `wind_velocity` | array | [0,0,0] | Velocidad del viento (m/s) |
| `wind_profile_type` | string | "constant" | Tipo de perfil de viento |
| `wind_shear_coefficient` | float | 0.2 | Coeficiente de cizalladura |
| `enable_turbulence` | bool | false | Activar turbulencia |
| `turbulence_intensity` | float | 0.1 | Intensidad de turbulencia |

### **Parámetros Gravitacionales**

| Parámetro | Tipo | Valor por Defecto | Descripción |
|-----------|------|-------------------|-------------|
| `enable_gravity_variation` | bool | true | Variación gravitacional |
| `earth_radius` | float | 6371000.0 | Radio terrestre (m) |
| `standard_gravity` | float | 9.80665 | Gravedad estándar (m/s²) |
| `enable_coriolis` | bool | false | Efectos Coriolis |
| `earth_rotation_rate` | float | 7.2921e-5 | Velocidad rotación (rad/s) |

### 📊 **Flujo de Cálculo**

```c
int32_t plugin_tick(PluginHandle handle, PluginTickData* data) {
    EnvironmentPlugin* plugin = (EnvironmentPlugin*)handle;
    
    // 1. Obtener estado del vehículo
    Vector3 position = get_position(data->state);
    Vector3 velocity = get_velocity(data->state);
    float altitude = position.z;
    float sim_time = get_simulation_time(data->state);
    
    // 2. Calcular propiedades atmosféricas
    AtmosphericProperties atm = calculate_atmospheric_properties(plugin, altitude);
    
    // 3. Calcular efectos de viento
    Vector3 wind_force = {0, 0, 0};
    if (plugin->enable_wind) {
        Vector3 wind_velocity = calculate_wind_velocity(plugin, altitude, sim_time);
        wind_force = calculate_wind_effects(atm, velocity, wind_velocity);
    }
    
    // 4. Calcular efectos gravitacionales
    Vector3 gravity_force = calculate_gravity_effects(plugin, position, velocity);
    
    // 5. Combinar todas las fuerzas ambientales
    Vector3 total_force = vector3_add(wind_force, gravity_force);
    
    // 6. Actualizar salida
    data->output_force->x = total_force.x;
    data->output_force->y = total_force.y;
    data->output_force->z = total_force.z;
    
    return 0;
}
```