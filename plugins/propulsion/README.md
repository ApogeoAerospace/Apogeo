# Propulsion plugin

The propulsion plugin is currently in its architecture and interface-definition
phase. Its design separates the host ABI, state/configuration mapping, propulsion
orchestration, engine models, and propellant storage.

## Design deliverables

- [Architecture](docs/ARCHITECTURE.md)
- [Editable UML source](docs/propulsion_class_diagram.puml)
- [Rendered UML diagram](docs/propulsion_class_diagram.png)
- [Initial C++ interfaces](include/propulsion/interfaces.hpp)
- [Initial model hierarchy](include/propulsion/models.hpp)
- [Host boundary adapter](include/propulsion/plugin_adapter.hpp)

No production propulsion implementation is introduced by this task. The headers
define the intended contracts for the implementation and test phases.
