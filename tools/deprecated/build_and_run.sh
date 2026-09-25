#!/bin/bash

# MoLab Build and Run Script
# Simplified version for clean codebase

set -e

echo "=== MoLab Aerospace Simulator - Build and Run ==="

# Change to project root
cd "$(dirname "$0")/.."

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Configure and build
echo "Configuring CMake..."
cmake ..

echo "Building MoLab..."
make -j4

echo "Build completed successfully!"

# Run options
echo ""
echo "Available run options:"
echo "1. Basic simulation (no plugins): ./bin/simulator --config ../data/config/basic_config.json --ticks 50"
echo "2. Force simulation (with plugins): ./bin/simulator --config ../data/config/main_config.json --ticks 100"
echo "3. Debug mode: Add --log-level DEBUG to any command"

# Ask user what to run
echo ""
read -p "Run simulation now? (1=basic, 2=force, n=no): " choice

case $choice in
    1)
        echo "Running basic simulation..."
        ./bin/simulator --config ../data/config/basic_config.json --ticks 50
        ;;
    2)
        echo "Running force simulation..."
        ./bin/simulator --config ../data/config/main_config.json --ticks 100
        ;;
    n|N)
        echo "Build completed. Run manually when ready."
        ;;
    *)
        echo "Invalid choice. Build completed. Run manually when ready."
        ;;
esac

echo ""
echo "Output files are in: build/output/"
echo "Logs are in: build/logs/"
echo ""
echo "To analyze results, run: python3 tools/analyze_results.py build/output/[filename].csv"
