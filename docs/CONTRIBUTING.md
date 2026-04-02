## Flujo de trabajo de desarrollo

1. **Crear un issue**: antes de iniciar una nueva funcionalidad o corrección, abre un issue para discutir los cambios propuestos.

2. **Ramas**:
   - Todo trabajo nuevo debe hacerse en una rama de funcionalidad.
   - Crea la rama desde `develop`:
     ```bash
     git checkout develop
     git pull origin develop
     git checkout -b feature/nombre-de-tu-funcionalidad
     ```

3. **Realizar cambios**:
   - Haz commits en tu rama de funcionalidad.
   - Usa mensajes de commit claros y descriptivos.

4. **Subir cambios**:
   - Sube tu rama al repositorio remoto:
     ```bash
     git push origin feature/nombre-de-tu-funcionalidad
     ```

5. **Crear pull request**:
   - Abre un pull request desde tu rama hacia `develop`.
   - Referencia el issue relacionado en la descripción.
   - Asegura al menos una revisión antes de hacer merge.

## Estilo de código

1. **Convenciones de nombres**:
   - Variables y funciones: `snake_case`.
   - Clases: `PascalCase`.
   - Constantes: `UPPER_CASE_SNAKE_CASE`.
