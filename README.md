# MoLab Aerospace Simulator

Modular aerospace simulation engine with plugin architecture and real-time force integration.

## Requirements

- C++17-compatible compiler
- CMake 3.20 or newer
- Python 3 (interfaces and utilities)

## Quick build

```bash
cmake -B build -S .
cmake --build build --parallel
```

## Execution

### Web interface (recommended)

```bash
./scripts/launch_web.sh
```

Open `http://localhost:8082`.

### Smart launcher

```bash
./launch_molab.sh
```

### Desktop GUI

```bash
./launch_gui.sh
```

### Command line

```bash
build/bin/simulator --config data/defaults/default_config.json --ticks 50
```

## Documentation

- Project documentation index: `docs/README.md`
- Doxygen style guide: `docs/DOCUMENTATION_STYLE_GUIDE.md`
- Generated Doxygen HTML output: `docs/generated/html/index.html`

## Repository structure

```text
MoLab/
├── src/                  # Main source code
├── tests/                # Unit tests (Google Test)
├── docs/                 # Technical and functional documentation
├── tools/                # Utilities and auxiliary servers
├── data/                 # Configuration and initial states
└── plugins/              # Simulation plugins
```

## License

Project under GNU AFFERO GENERAL PUBLIC LICENSE V3
