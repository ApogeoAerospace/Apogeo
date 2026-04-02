# Guía de Estilo de Documentación - MoLab

> Estado: **Implementado (guía oficial vigente)**

Este documento establece el estándar oficial para la documentación dentro del código fuente del proyecto MoLab. Todo el equipo debe seguir estas convenciones para mantener consistencia y facilitar la generación automática de documentación.

## Herramienta Oficial: Doxygen

MoLab utiliza **Doxygen** como herramienta de documentación. Doxygen lee comentarios especiales en el código y genera documentación HTML, PDF o LaTeX automáticamente.

### Instalación de Doxygen

**Windows:**
```bash
# Usando winget
winget install doxygen

# O descargar desde: https://www.doxygen.nl/download.html
```

**Linux:**
```bash
sudo apt install doxygen doxygen-gui graphviz
```

**macOS:**
```bash
brew install doxygen graphviz
```

### Generar la documentación

Desde la raíz del proyecto:
```bash
doxygen Doxyfile
```

La documentación se generará en `docs/generated/html/index.html`.

---

## Formato de comentarios

### Estilo oficial: Javadoc con `/**`

Usamos el estilo Javadoc con doble asterisco para todos los comentarios de documentación:

```cpp
/**
 * @brief Descripción corta de una línea.
 *
 * Descripción más detallada del elemento. Puede ocupar
 * múltiples líneas y explicar el comportamiento en profundidad.
 */
```

### Idioma

- **Comentarios de documentación**: Español
- **Nombres de variables, clases y métodos**: Inglés (como el código existente)

---

## Documentación de clases

Toda clase pública debe tener un bloque de documentación antes de su declaración.

### Plantilla

```cpp
/**
 * @class NombreClase
 * @brief Descripción breve de la clase.
 *
 * Descripción detallada de la responsabilidad de la clase,
 * su propósito en el sistema y cómo se relaciona con otros componentes.
 *
 * @author Nombre del autor (opcional)
 * @date Fecha de creación (opcional)
 *
 * @see ClaseRelacionada
 */
class NombreClase {
    // ...
};
```

---

## Documentación de métodos y funciones

### Plantilla

```cpp
/**
 * @brief Descripción breve del método.
 *
 * Descripción detallada del comportamiento, incluyendo
 * casos especiales y efectos secundarios.
 *
 * @param nombre_parametro Descripción del parámetro.
 * @param otro_parametro Descripción de otro parámetro.
 *
 * @return Descripción del valor de retorno.
 *
 * @throws TipoExcepcion Cuándo se lanza esta excepción.
 *
 * @pre Precondición que debe cumplirse antes de llamar.
 * @post Postcondición garantizada después de ejecutar.
 *
 * @note Notas adicionales importantes.
 * @warning Advertencias sobre uso incorrecto.
 */
```

---

## Comandos Doxygen de referencia rápida

| Comando | Uso |
|---------|-----|
| `@brief` | Descripción corta (una línea) |
| `@param` | Documentar un parámetro |
| `@return` | Documentar el valor de retorno |
| `@throws` | Documentar excepciones |
| `@see` | Referencia a elementos relacionados |
| `@note` | Nota informativa |
| `@warning` | Advertencia importante |
| `@deprecated` | Marcar como obsoleto |
| `@todo` | Tareas pendientes |
| `@bug` | Bugs conocidos |

---

## Lista de verificación

Antes de hacer commit, verifica:

- [ ] ¿Todas las clases públicas tienen `@brief` y descripción?
- [ ] ¿Todos los métodos públicos tienen `@param` y `@return`?
- [ ] ¿Los archivos nuevos tienen bloque `@file`?
- [ ] ¿La documentación está en español?
- [ ] ¿`doxygen Doxyfile` ejecuta sin warnings relevantes?

---

## Documento de referencia histórica

La versión histórica se conserva en:

- [Versión depredada de la guía](./deprecated/DOCUMENTATION_STYLE_GUIDE.md)
