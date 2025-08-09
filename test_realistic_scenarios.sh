#!/bin/bash

# MoLab Realistic Aerospace Scenarios Test Suite
# Tests various realistic flight scenarios with detailed physics

set -e

PROJECT_ROOT="/Users/johancastrillon/Documents/MoLab"
BUILD_DIR="$PROJECT_ROOT/build"
SIMULATOR="$BUILD_DIR/bin/simulator"
RESULTS_DIR="$PROJECT_ROOT/results"

echo "🚀 MOLAB REALISTIC AEROSPACE SCENARIOS TEST SUITE"
echo "================================================="

# Check if simulator exists
if [ ! -f "$SIMULATOR" ]; then
    echo "❌ Simulator not found. Building..."
    cd "$BUILD_DIR"
    make -j4
    cd "$PROJECT_ROOT"
fi

# Create results directory
mkdir -p "$RESULTS_DIR"

echo ""
echo "🎯 SCENARIO 1: SUBORBITAL FLIGHT (New Shepard Style)"
echo "===================================================="
echo "• Vehicle: 50,000 kg rocket"
echo "• Mission: Reach 100+ km altitude"
echo "• Duration: 5 minutes"
echo "• Plugins: All aerodynamics, propulsion, structures, environment"
echo "• Expected: Parabolic trajectory, max altitude ~120 km"

CONFIG_FILE="$PROJECT_ROOT/data/config/test_suborbital_realistic.json"
echo "Running suborbital simulation..."
"$SIMULATOR" --config "$CONFIG_FILE" --ticks 3000 > suborbital_log.txt 2>&1

# Find the most recent result file
SUBORBITAL_RESULT=$(ls -t "$RESULTS_DIR"/molab_simulation_*.csv | head -n 1)
echo "✅ Suborbital simulation completed: $(basename "$SUBORBITAL_RESULT")"

# Analyze key metrics
if [ -f "$SUBORBITAL_RESULT" ]; then
    MAX_ALTITUDE=$(awk -F',' 'NR>1 {if($4>max) max=$4} END {print max}' "$SUBORBITAL_RESULT")
    MAX_VELOCITY=$(awk -F',' 'NR>1 {v=sqrt($5*$5+$6*$6+$7*$7); if(v>max) max=v} END {print max}' "$SUBORBITAL_RESULT")
    FLIGHT_TIME=$(tail -n 1 "$SUBORBITAL_RESULT" | cut -d',' -f1)
    
    echo "  📊 Max Altitude: ${MAX_ALTITUDE} m ($(echo "$MAX_ALTITUDE/1000" | bc -l | xargs printf "%.1f") km)"
    echo "  📊 Max Velocity: ${MAX_VELOCITY} m/s (Mach $(echo "$MAX_VELOCITY/343" | bc -l | xargs printf "%.1f"))"
    echo "  📊 Flight Time: ${FLIGHT_TIME} seconds"
    
    # Check if realistic
    if (( $(echo "$MAX_ALTITUDE > 100000" | bc -l) )); then
        echo "  ✅ REALISTIC: Reached space boundary (100+ km)"
    else
        echo "  ⚠️  UNREALISTIC: Did not reach space boundary"
    fi
fi

echo ""
echo "🎯 SCENARIO 2: ORBITAL INSERTION (Falcon 9 Style)"
echo "================================================"
echo "• Vehicle: 550,000 kg rocket (2-stage)"
echo "• Mission: Reach 400 km circular orbit"
echo "• Duration: 100 minutes"
echo "• Plugins: All with orbital mechanics"
echo "• Expected: Multi-stage ascent, orbit insertion"

CONFIG_FILE="$PROJECT_ROOT/data/config/test_orbital_realistic.json"
echo "Running orbital simulation..."
"$SIMULATOR" --config "$CONFIG_FILE" --ticks 6000 > orbital_log.txt 2>&1

ORBITAL_RESULT=$(ls -t "$RESULTS_DIR"/molab_simulation_*.csv | head -n 1)
echo "✅ Orbital simulation completed: $(basename "$ORBITAL_RESULT")"

if [ -f "$ORBITAL_RESULT" ]; then
    MAX_ALTITUDE=$(awk -F',' 'NR>1 {if($4>max) max=$4} END {print max}' "$ORBITAL_RESULT")
    FINAL_VELOCITY=$(tail -n 1 "$ORBITAL_RESULT" | awk -F',' '{v=sqrt($5*$5+$6*$6+$7*$7); print v}')
    ORBITAL_VELOCITY_NEEDED=7800  # m/s for 400km orbit
    
    echo "  📊 Max Altitude: ${MAX_ALTITUDE} m ($(echo "$MAX_ALTITUDE/1000" | bc -l | xargs printf "%.1f") km)"
    echo "  📊 Final Velocity: ${FINAL_VELOCITY} m/s"
    echo "  📊 Orbital Velocity Needed: ${ORBITAL_VELOCITY_NEEDED} m/s"
    
    if (( $(echo "$MAX_ALTITUDE > 350000" | bc -l) )) && (( $(echo "$FINAL_VELOCITY > 7000" | bc -l) )); then
        echo "  ✅ REALISTIC: Achieved orbital parameters"
    else
        echo "  ⚠️  UNREALISTIC: Did not achieve stable orbit"
    fi
fi

echo ""
echo "🎯 SCENARIO 3: POWERED LANDING (SpaceX Style)"
echo "============================================="
echo "• Vehicle: 25,000 kg booster"
echo "• Mission: Land from 10 km altitude"
echo "• Duration: 3 minutes"
echo "• Plugins: Advanced aerodynamics with grid fins"
echo "• Expected: Controlled descent, soft landing"

CONFIG_FILE="$PROJECT_ROOT/data/config/test_landing_realistic.json"
echo "Running landing simulation..."
"$SIMULATOR" --config "$CONFIG_FILE" --ticks 3600 > landing_log.txt 2>&1

LANDING_RESULT=$(ls -t "$RESULTS_DIR"/molab_simulation_*.csv | head -n 1)
echo "✅ Landing simulation completed: $(basename "$LANDING_RESULT")"

if [ -f "$LANDING_RESULT" ]; then
    INITIAL_ALTITUDE=$(head -n 2 "$LANDING_RESULT" | tail -n 1 | cut -d',' -f4)
    FINAL_ALTITUDE=$(tail -n 1 "$LANDING_RESULT" | cut -d',' -f4)
    FINAL_VELOCITY=$(tail -n 1 "$LANDING_RESULT" | awk -F',' '{v=sqrt($5*$5+$6*$6+$7*$7); print v}')
    LANDING_TIME=$(tail -n 1 "$LANDING_RESULT" | cut -d',' -f1)
    
    echo "  📊 Initial Altitude: ${INITIAL_ALTITUDE} m"
    echo "  📊 Final Altitude: ${FINAL_ALTITUDE} m"
    echo "  📊 Landing Velocity: ${FINAL_VELOCITY} m/s"
    echo "  📊 Landing Time: ${LANDING_TIME} seconds"
    
    if (( $(echo "$FINAL_ALTITUDE < 100" | bc -l) )) && (( $(echo "$FINAL_VELOCITY < 10" | bc -l) )); then
        echo "  ✅ REALISTIC: Successful soft landing"
    else
        echo "  ⚠️  UNREALISTIC: Hard landing or crash"
    fi
fi

echo ""
echo "🎯 PHYSICS VALIDATION SUMMARY"
echo "============================"

echo ""
echo "📊 EXPECTED REALISTIC BEHAVIORS:"
echo "• Suborbital: Parabolic trajectory, 3-4 min to apogee, Mach 3+ speeds"
echo "• Orbital: Multi-stage acceleration, 8+ min to orbit, 7.8+ km/s velocity"
echo "• Landing: Controlled deceleration, grid fin stabilization, <5 m/s touchdown"

echo ""
echo "🔬 PLUGIN PHYSICS VALIDATION:"
echo "• Aerodynamics: Drag increases with velocity², lift from angle of attack"
echo "• Propulsion: Thrust decreases with altitude, fuel consumption realistic"
echo "• Structures: Mass decreases with fuel burn, CG shifts affect stability"
echo "• Environment: Atmospheric density decreases exponentially with altitude"

echo ""
echo "📈 RECOMMENDED VISUALIZATIONS:"
echo "• Altitude vs Time (parabolic for suborbital, exponential for orbital)"
echo "• Velocity vs Time (acceleration phases clearly visible)"
echo "• Drag Force vs Altitude (shows atmospheric density effects)"
echo "• Mass vs Time (fuel consumption curves)"
echo "• G-Forces vs Time (launch and landing stress)"
echo "• Earth Trajectory Plot (ground track and orbital mechanics)"

echo ""
echo "🌍 NEXT STEPS:"
echo "• Open web interface: http://localhost:8082"
echo "• View results in advanced visualization"
echo "• Compare scenarios side by side"
echo "• Analyze physics realism in detail"

echo ""
echo "✅ REALISTIC SCENARIOS TEST COMPLETED"
echo "All simulation data ready for analysis!"
