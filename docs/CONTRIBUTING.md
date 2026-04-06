## Flujo de trabajo de desarrollo
# Contributing

1. **Create an issue**: before starting a new feature or fix, open an issue to discuss the proposed changes.

2. **Branches**:
   - All new work must be done in a feature branch.
   - Create the branch from `develop`:
     ```bash
     git checkout develop
     git pull origin develop
     git checkout -b feature/your-feature-name
     ```

3. **Make changes**:
   - Commit your work in your feature branch.
   - Use clear and descriptive commit messages.

4. **Push changes**:
   - Push your branch to the remote repository:
     ```bash
     git push origin feature/your-feature-name
     ```

5. **Create pull request**:
   - Open a pull request from your branch to `develop`.
   - Reference the related issue in the description.
   - Ensure at least one review before merge.

## Code style

1. **Naming conventions**:
   - Variables and functions: `snake_case`.
   - Classes: `PascalCase`.
   - Constants: `UPPER_CASE_SNAKE_CASE`.
