# Casos de validación del modelo de propulsión

**Motor de referencia:** Merlin 1D (nivel del mar)

---

## 1. Introducción

Este documento define los casos de validación utilizados para verificar la precisión del modelo de propulsión desarrollado en este proyecto. La validación se basa en datos de desempeño públicamente disponibles del motor Merlin 1D. Cada caso especifica los parámetros de entrada, variables de salida, valores esperados de referencia y fuentes de datos.

---

## 2. Caso de validación 1: Empuje a nivel del mar

### Condiciones de entrada

- Presión de cámara
- Relación de mezcla (LOX/RP-1)
- Presión ambiente (condiciones a nivel del mar)

### Parámetros de entrada

| Parámetro                  | Valor        |
| -------------------------- | ------------ |
| `Chamber_Pressure`         | 9.7 MPa      |
| `Mixture_Ratio_LOX_RP1`    | 2.56         |
| `Ambient_Pressure`         | 101325 Pa    |
| `Gravity_Standard`         | 9.80665 m/s² |

### Parámetro de salida

- Empuje a nivel del mar

### Valor esperado

- **845,000 N**

### Fuente de referencia

- Guía de usuario del Falcon 9 de SpaceX

---

## 3. Caso de validación 2: Impulso específico en vacío

### Condiciones de entrada

- Características de expansión de la tobera
- Presión ambiente cercana al vacío

### Parámetros de entrada

| Parámetro                  | Valor        |
| -------------------------- | ------------ |
| `Chamber_Pressure`         | 9.7 MPa      |
| `Mixture_Ratio_LOX_RP1`    | 2.56         |
| `Ambient_Pressure`         | 0 Pa         |
| `Gravity_Standard`         | 9.80665 m/s² |

### Parámetro de salida

- Impulso específico en vacío

### Valor esperado

- **311 s**

### Fuente de referencia

- Guía de usuario del Falcon 9 de SpaceX

---

## 4. Caso de validación 3: Flujo másico de propelente

### Condiciones de entrada

- Empuje a nivel del mar
- Impulso específico a nivel del mar
- Aceleración gravitacional estándar

### Parámetros de entrada

| Parámetro                    | Valor        |
| ---------------------------- | ------------ |
| `Thrust_Sea_Level`           | 845000 N     |
| `Specific_Impulse_Sea_Level` | 282 s        |
| `Gravity_Standard`           | 9.80665 m/s² |

### Parámetro de salida

- Flujo másico de propelente

### Valor esperado

- **Aproximadamente 305 kg/s**

### Fuente de referencia

- Calculado con la ecuación clásica de empuje
- Sutton y Biblarz, *Rocket Propulsion Elements*

---

## 5. Notas

Los valores esperados utilizados en estos casos de validación se almacenan en el archivo de datos de referencia incluido en el repositorio del proyecto. Dependiendo del escenario de validación, los parámetros pueden actuar como entradas o salidas. Este enfoque permite flexibilidad para evaluar distintos aspectos del modelo de propulsión.
