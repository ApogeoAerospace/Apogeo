#!/bin/bash

# MoLab Web Interface Launcher
# Starts a local web server for the MoLab GUI

set -e

echo "🌐 Starting MoLab Web Interface..."

# Change to project root
cd "$(dirname "$0")"

# Check if Python 3 is available
if ! command -v python3 &> /dev/null; then
    echo "❌ Error: Python 3 is required but not found."
    echo "Please install Python 3 and try again."
    exit 1
fi

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "⚠️  Build directory not found. Building MoLab first..."
    if [ -f "tools/build_and_run.sh" ]; then
        echo "n" | ./tools/build_and_run.sh
    else
        echo "❌ Error: Build script not found. Please build MoLab manually."
        exit 1
    fi
fi

# Check if simulator exists
if [ ! -f "build/bin/simulator" ]; then
    echo "❌ Error: MoLab simulator not found in build/bin/"
    echo "Please build the project first using: ./tools/build_and_run.sh"
    exit 1
fi

echo "✅ All dependencies ready"
echo "🚀 Launching web interface..."
echo ""
echo "📋 Instructions:"
echo "   • The web interface will open automatically in your browser"
echo "   • If it doesn't open, go to: http://localhost:8080"
echo "   • Use Ctrl+C to stop the server"
echo ""

# Launch web interface
python3 tools/molab_web_gui.py
