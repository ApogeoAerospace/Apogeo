# MoLab Documentation

This directory contains the project's technical and functional documentation.

## Structure

- `docs/*.md`: active top-level documentation aligned with current code.
- `docs/specs/`: feature-level specifications and task breakdowns.
- `docs/deprecated/`: historical material kept for traceability.
- `docs/generated/`: generated API/reference output (Doxygen).

## Main links

- [Build and run](./BUILD_RUN.md)
- [Current architecture](./ARCHITECTURE.md)
- [Current configuration](./CONFIG.md)
- [Configuration reference](./CONFIG_REFERENCE.md)
- [IPC protocol](./IPC_PROTOCOL.md)
- [Plugin API](./PLUGIN_API.md)
- [Structures module](./STRUCTURES_MODULE.md)
- [Known gaps](./KNOWN_GAPS.md)
- [Doxygen style guide](./DOXYGEN_STYLE.md)
- [Documentation writing guide](./DOCUMENTATION_STYLE_GUIDE.md)

## Generated documentation

Doxygen HTML output is generated at:

- `docs/generated/html/index.html`

## Documentation status convention

Each document should be marked in-content according to its status:

- `Current`
- `Planned`
- `Historical`
