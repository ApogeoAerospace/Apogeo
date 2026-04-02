# Guía de estilo Doxygen (adoptada)

Usa este estilo para todos los encabezados públicos en `src/core/` y `src/api/`.

## Plantilla de clase

```cpp
/**
 * @brief Descripción corta de una línea.
 *
 * Resumen más detallado del comportamiento a nivel de contrato.
 */
class Example {
public:
    /**
     * @brief Ejecuta un paso de simulación.
     * @param dt Delta de tiempo de simulación en segundos.
     * @return true si fue exitoso, false en caso contrario.
     * @note Thread-safe.
     */
    bool step(double dt);
};
```

## Reglas

1. API pública: siempre usar bloques Doxygen.
2. Usar etiquetas de forma consistente:
   - `@brief`
   - `@param`
   - `@return`
   - `@note`
   - `@warning`
3. Incluir unidades en la documentación de parámetros.
4. Mantener detalles de runtime/algoritmo en `.cpp`, no en comentarios de encabezados.
5. Idioma estándar para documentación del proyecto: español.

## Referencia oficial

La guía oficial del proyecto está en:

- [`docs/DOCUMENTATION_STYLE_GUIDE.md`](../DOCUMENTATION_STYLE_GUIDE.md)

---

Estado de documentación: **Current**

Volver a: [`docs/README.md`](../README.md)
