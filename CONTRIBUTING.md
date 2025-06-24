## Development Workflow

1.  **Crear un Issue**: Antes de comenzar a trabajar en una nueva funcionalidad o corrección de errores, por favor cree un issue para discutir los cambios propuestos.

2.  **Ramas**:
    * Todo el trabajo nuevo debe realizarse en una rama de funcionalidad.
    * Cree su rama de funcionalidad a partir de la rama `develop`:
        ```bash
        git checkout develop
        git pull origin develop
        git checkout -b feature/nombre-de-su-funcionalidad
        ```

3.  **Realización de Cambios**:
    * Realice sus cambios en la rama de funcionalidad.
    * Realice commits de sus cambios con un mensaje claro y descriptivo.

4.  **Subir Cambios**:
    * Suba su rama de funcionalidad al repositorio remoto:
        ```bash
        git push origin feature/nombre-de-su-funcionalidad
        ```

5.  **Crear un Pull Request**:
    * Cree un pull request desde su rama de funcionalidad hacia la rama `develop`.
    * En la descripción del pull request, haga referencia al issue que creó.
    * Asegúrese de que su pull request sea revisado por al menos otro miembro del equipo antes de hacer el merge.

## Estilo de Código:

1. **Convenciones de Nombres**:
    * Variables y Funciones: Utilice snake_case (mi_variable).
    * Clases: Utilice PascalCase (o CapWords) para todos los lenguajes (MiClase).
    * Constantes: Utilice UPPER_CASE_SNAKE_CASE (MI_CONSTANTE).
