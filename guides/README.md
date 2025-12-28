# MoLab Aerospace Simulator

A modular aerospace simulation engine with plugin-based physics calculations and real-time force integration.

## Features

- **Universal Web Interface**: Modern web GUI that works in any environment without external dependencies
- **Traditional GUI**: Desktop interface with Tkinter for local use
- **Interactive CLI**: Command-line interface with full functionality
- **Modular Plugin Architecture**: Extensible physics calculations through plugins
- **Real-Time Force Integration**: Parallel physics calculators with thread-safe force aggregation
- **Advanced Time Management**: UTC timestamps and simulation time tracking
- **Multiple Integration Methods**: Euler, Runge-Kutta 4, and Verlet integrators
- **Comprehensive Output**: CSV and JSON data export with detailed logging
- **Thread-Safe Design**: Concurrent plugin execution with mutex protection

## Quick Start

### Prerequisites

- C++17 compatible compiler
- CMake 3.20 or higher
- Python 3.x (for interfaces and analysis tools)

### Option 1: Web Interface (Universal - Recommended)

**Works everywhere: SSH, Docker, Cloud, Mobile, Desktop**

1. **Launch web interface**:
   ```bash
   git clone https://github.com/Samuelbomc/MoLab
   cd MoLab
   ./launch_web.sh
   ```

2. **Open in browser**: `http://localhost:8080`

3. **Configure and run simulations** through the modern web interface

### Option 2: Smart Launcher (Auto-detects best interface)

```bash
./launch_molab.sh
```
**Automatically chooses between GUI, CLI, or Web based on your environment**

### Option 3: Traditional GUI (Desktop only)

```bash
./launch_gui.sh
```

### Option 4: Command Line

1. **Build and run with the automated script**:
   ```bash
   ./tools/build_and_run.sh
   ```

2. **Or build manually**:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j4
   ```

### Running Simulations

**Using the Web Interface** (Universal):
```bash
./launch_web.sh
# Open http://localhost:8080 in any browser
```

**Using the Smart Launcher**:
```bash
./launch_molab.sh
```

**Command line options**:

**Basic simulation (gravity only)**:
```bash
./bin/simulator --config ../data/config/basic_config.json --ticks 50
```

**Advanced simulation (with force plugins)**:
```bash
./bin/simulator --config ../data/config/main_config.json --ticks 100
```

## User Interfaces

### Web Interface (Recommended)
- **Universal compatibility**: Works in SSH, Docker, Cloud, Mobile
- **No dependencies**: Only needs Python 3 and a browser
- **Modern design**: Responsive interface with real-time updates
- **Full functionality**: Complete simulation configuration and control

**Launch**: `./launch_web.sh` → Open `http://localhost:8080`

### Desktop GUI
- **Rich interface**: Full Tkinter-based desktop application
- **Tabbed organization**: Simulation, Physics, Plugins, Output tabs
- **Local execution**: Direct integration with system

**Launch**: `./launch_gui.sh`

### Interactive CLI
- **Terminal-based**: Full-featured command-line interface
- **Menu-driven**: Easy navigation through options
- **Universal**: Works in any terminal environment

**Launch**: `python3 tools/molab_cli_gui.py`

### Smart Launcher
- **Auto-detection**: Chooses best interface for your environment
- **Fallback support**: Gracefully handles missing dependencies

**Launch**: `./launch_molab.sh`

## Graphical User Interface

The MoLab GUI provides an intuitive interface with the following tabs:

### Simulation Tab
- Time step configuration
- Simulation duration
- Maximum iterations
- Logging settings

### Physics Tab  
- Gravity settings
- Atmospheric drag options
- Integration method selection
- Numerical tolerance

### Plugins Tab
- Available plugins list
- Plugin descriptions
- Enable/disable plugins
- Plugin information

### Output Tab
- Output format selection (CSV, JSON, Binary)
- Output interval settings
- Output directory configuration

### Control Panel
- Load/Save configurations
- Run/Stop simulation
- View results
- Real-time status

## Configuration

### Main Configuration (`main_config.json`)
- Complete simulation with force plugins
- Runge-Kutta 4 integration
- Full logging and validation

### Basic Configuration (`basic_config.json`)
- Simple gravity-only simulation
- Euler integration
- Minimal logging

## Plugin Development

Create custom physics plugins by implementing the Plugin API:

```cpp
extern "C" {
    PluginHandle* plugin_create_instance();
    void plugin_destroy_instance(PluginHandle* handle);
    int plugin_tick(PluginHandle* handle, uint8_t* state_buffer, 
                   size_t buffer_size, PluginVector3* force, PluginVector3* torque);
}
```

See `plugins/test_force/` for a complete example.

## Output Analysis

**Using the GUI**: Click "View Results" button after simulation

**Command line**:
```bash
python3 tools/analyze_results.py build/output/simulation_results.csv
```

## Project Structure

```
MoLab/
├── tools/
│   ├── molab_gui.py          # Desktop GUI (Tkinter)
│   ├── build_and_run.sh      # Build script
│   └── analyze_results.py    # Results analysis
├── core/                     # Core simulation engine
├── src/                      # Source files and API
├── plugins/                  # Physics calculation plugins
├── data/                     # Configuration and initial states
├── launch_gui.sh             # GUI launcher
└── build/                    # Build output and results
```

## Screenshots

The GUI provides:
- **Tabbed interface** for organized parameter configuration
- **Real-time status** updates during simulation
- **Integrated results viewer** for immediate analysis
- **Configuration management** with save/load functionality

## License

This project is licensed under the MIT License.
