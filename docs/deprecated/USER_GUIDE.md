# 🚀 MoLab Universal GUI - User Guide

## 🌟 **Quick Start**

### **🌐 Web Interface (Recommended)**
```bash
./launch_web.sh
# Opens: http://localhost:8082
```

### **🖥️ Desktop GUI**
```bash
./launch_gui.sh
```

### **⌨️ CLI Interface**
```bash
python3 tools/molab_cli_gui.py
```

## 📋 **Web Interface Features**

### **⚙️ Simulation Tab**
- **Time Step**: Integration step size (0.001 - 1.0 seconds)
- **Duration**: Total simulation time (1 - 1000 seconds)
- **Max Iterations**: Safety limit (100 - 100000)
- **Logging**: Enable/disable with log level control

### **⚗️ Physics Tab**
- **Gravity**: Enable/disable gravitational effects
- **Atmospheric Drag**: Air resistance simulation
- **Wind Effects**: Environmental wind forces
- **Integrator**: Runge-Kutta 4, Euler, or Verlet
- **Tolerance**: Integration accuracy (1e-8 to 1e-3)

### **🔌 Plugins Tab**
- **Auto-Detection**: Finds plugins in `build/plugins/`
- **Enable/Disable**: Toggle plugins on/off
- **Real-time Updates**: Changes apply immediately

### **📊 Output Tab**
- **CSV Output**: Position, velocity, time data
- **JSON Output**: Complete state information
- **Binary Output**: High-performance format
- **Output Interval**: Data sampling frequency

### **📈 Results Tab**
- **Interactive Charts**: Position vs time plots
- **Statistics**: Duration, final position, data points
- **Multiple Results**: View and compare different runs
- **Auto-Refresh**: Updates after each simulation

## 🎯 **Typical Workflow**

### **1. Basic Gravity Test**
```
1. Simulation Tab → Duration: 5s, Time Step: 0.1s
2. Physics Tab → Enable Gravity only
3. Plugins Tab → Disable all plugins
4. Output Tab → Enable CSV output
5. Run Simulation → View Results
```

### **2. Plugin Force Test**
```
1. Simulation Tab → Duration: 10s, Time Step: 0.01s
2. Physics Tab → Enable Gravity + Drag
3. Plugins Tab → Enable "Force Generator"
4. Output Tab → Enable CSV + JSON
5. Run Simulation → Analyze complex motion
```

## 🔧 **Configuration Management**

### **Save Configuration**
- Click "💾 Save Config" in Control Panel
- Enter filename (e.g., "gravity_test")
- Config saved to `data/config/`

### **Load Configuration**
- Click "📂 Load Config" in Control Panel
- Select from dropdown list
- All parameters loaded automatically

## 📊 **Results Visualization**

### **Chart Features**
- **Interactive**: Zoom, pan, hover tooltips
- **Multi-series**: X, Y, Z positions simultaneously
- **Responsive**: Works on desktop and mobile
- **Export Ready**: Screenshots and data download

### **Statistics Display**
- **Duration**: Total simulation time
- **Data Points**: Number of samples recorded
- **Final Position**: End coordinates (X, Y, Z)
- **File Info**: Size, modification date

## 🛠️ **Troubleshooting**

### **Common Issues**

**"System not ready"**
```bash
# Build the project first
mkdir -p build && cd build
cmake .. && make -j$(nproc)
```

**"No plugins found"**
```bash
# Ensure plugins are built
cd build && make plugins
```

**"Port already in use"**
```bash
# Kill existing processes
pkill -f "python3.*molab_web_gui"
```

**Web interface not loading**
```bash
# Check server status
lsof -i :8082
# Restart server
python3 tools/molab_web_gui.py
```

## 🎯 **Advanced Features**

### **Plugin Development**
- Create plugins in `plugins/` directory
- Follow existing plugin structure
- Automatic detection after build

### **Custom Configurations**
- JSON format in `data/config/`
- All parameters configurable
- Version control friendly

### **Batch Processing**
- Use CLI interface for automation
- Script multiple simulations
- Integrate with CI/CD pipelines

## 📁 **File Structure**

```
MoLab/
├── tools/
│   ├── molab_web_gui.py    # Web interface
│   ├── molab_gui.py        # Desktop GUI
│   └── molab_cli_gui.py    # CLI interface
├── data/config/            # Configuration files
├── output/                 # Simulation results
├── build/
│   ├── bin/simulator       # Main executable
│   └── plugins/            # Compiled plugins
└── launch_*.sh             # Launcher scripts
```

## 🎉 **Tips for Best Results**

### **Performance**
- Use smaller time steps for accuracy
- Enable only needed plugins
- Monitor output file sizes

### **Visualization**
- Enable CSV output for charts
- Use appropriate simulation duration
- Save configs for reproducibility

### **Development**
- Use CLI for automated testing
- Desktop GUI for detailed analysis
- Web interface for demos and sharing

---

## ✅ **System Status**

**🎉 FULLY FUNCTIONAL:**
- ✅ Universal web interface
- ✅ Interactive result visualization
- ✅ Cross-platform compatibility
- ✅ Plugin system integration
- ✅ Configuration management
- ✅ Error handling and recovery

**🌐 Access: http://localhost:8082**
