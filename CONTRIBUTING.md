# Contributing to MoLab

We're excited that you want to contribute! This document outlines the process for contributing to the project.

## Development Workflow

1.  **Create an Issue**: Before starting work on a new feature or bugfix, please create an issue in the issue tracker to discuss the proposed changes.

2.  **Branching**:
    * All new work should be done in a feature branch.
    * Create your feature branch from the `develop` branch:
        ```bash
        git checkout develop
        git pull origin develop
        git checkout -b feature/your-feature-name
        ```

3.  **Making Changes**:
    * Make your changes in the feature branch.
    * Commit your changes with a clear and descriptive commit message.

4.  **Pushing Changes**:
    * Push your feature branch to the remote repository:
        ```bash
        git push origin feature/your-feature-name
        ```

5.  **Creating a Pull Request**:
    * Create a pull request from your feature branch to the `develop` branch.
    * In the pull request description, reference the issue you created.
    * Ensure your pull request is reviewed by at least one other team member before merging.

## Code Style

[Add any code style guidelines here, e.g., "This project follows the XYZ style guide." You can also link to a style guide document.]

## Questions?

If you have any questions, feel free to ask in the project's communication channel (e.g., Slack, Discord, etc.).
