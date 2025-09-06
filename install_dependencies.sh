#!/bin/bash

# MoLab Dependencies Installer
# Installs required dependencies for MoLab Aerospace Simulator

set -e

echo "🔧 Installing MoLab Dependencies..."

# Check operating system
OS="$(uname -s)"
case "${OS}" in
    Linux*)     MACHINE=Linux;;
    Darwin*)    MACHINE=Mac;;
    CYGWIN*)    MACHINE=Cygwin;;
    MINGW*)     MACHINE=MinGw;;
    *)          MACHINE="UNKNOWN:${OS}"
esac

echo "📋 Detected OS: ${MACHINE}"

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check and install CMake
echo "🔍 Checking CMake..."
if command_exists cmake; then
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    echo "✅ CMake found: $CMAKE_VERSION"
    
    # Check if version is >= 3.20
    if [ "$(printf '%s\n' "3.20" "$CMAKE_VERSION" | sort -V | head -n1)" = "3.20" ]; then
        echo "✅ CMake version is sufficient"
    else
        echo "⚠️  CMake version $CMAKE_VERSION is too old (need >= 3.20)"
        echo "Please update CMake manually"
    fi
else
    echo "❌ CMake not found"
    if [ "$MACHINE" = "Mac" ]; then
        if command_exists brew; then
            echo "📦 Installing CMake via Homebrew..."
            brew install cmake
        else
            echo "Please install Homebrew first: https://brew.sh/"
            echo "Then run: brew install cmake"
        fi
    elif [ "$MACHINE" = "Linux" ]; then
        echo "Please install CMake:"
        echo "  Ubuntu/Debian: sudo apt-get install cmake"
        echo "  CentOS/RHEL: sudo yum install cmake"
        echo "  Fedora: sudo dnf install cmake"
    fi
fi

# Check Python 3
echo "🔍 Checking Python 3..."
if command_exists python3; then
    PYTHON_VERSION=$(python3 --version | cut -d' ' -f2)
    echo "✅ Python 3 found: $PYTHON_VERSION"
    
    # Check if tkinter is available
    if python3 -c "import tkinter" 2>/dev/null; then
        echo "✅ Tkinter module available"
    else
        echo "❌ Tkinter not available"
        if [ "$MACHINE" = "Mac" ]; then
            echo "Tkinter should be included with Python on macOS"
            echo "If missing, reinstall Python or install via Homebrew"
        elif [ "$MACHINE" = "Linux" ]; then
            echo "Install tkinter:"
            echo "  Ubuntu/Debian: sudo apt-get install python3-tk"
            echo "  CentOS/RHEL: sudo yum install tkinter"
            echo "  Fedora: sudo dnf install python3-tkinter"
        fi
    fi
else
    echo "❌ Python 3 not found"
    if [ "$MACHINE" = "Mac" ]; then
        if command_exists brew; then
            echo "📦 Installing Python 3 via Homebrew..."
            brew install python
        else
            echo "Please install Homebrew first: https://brew.sh/"
            echo "Then run: brew install python"
        fi
    elif [ "$MACHINE" = "Linux" ]; then
        echo "Please install Python 3:"
        echo "  Ubuntu/Debian: sudo apt-get install python3 python3-tk"
        echo "  CentOS/RHEL: sudo yum install python3 tkinter"
        echo "  Fedora: sudo dnf install python3 python3-tkinter"
    fi
fi

# Check C++ compiler
echo "🔍 Checking C++ compiler..."
if command_exists g++; then
    GCC_VERSION=$(g++ --version | head -n1)
    echo "✅ g++ found: $GCC_VERSION"
elif command_exists clang++; then
    CLANG_VERSION=$(clang++ --version | head -n1)
    echo "✅ clang++ found: $CLANG_VERSION"
else
    echo "❌ No C++ compiler found"
    if [ "$MACHINE" = "Mac" ]; then
        echo "Install Xcode Command Line Tools:"
        echo "  xcode-select --install"
    elif [ "$MACHINE" = "Linux" ]; then
        echo "Install build tools:"
        echo "  Ubuntu/Debian: sudo apt-get install build-essential"
        echo "  CentOS/RHEL: sudo yum groupinstall 'Development Tools'"
        echo "  Fedora: sudo dnf groupinstall 'Development Tools'"
    fi
fi

# Check Git
echo "🔍 Checking Git..."
if command_exists git; then
    GIT_VERSION=$(git --version)
    echo "✅ Git found: $GIT_VERSION"
else
    echo "❌ Git not found"
    if [ "$MACHINE" = "Mac" ]; then
        echo "Git should be included with Xcode Command Line Tools"
        echo "If missing: xcode-select --install"
    elif [ "$MACHINE" = "Linux" ]; then
        echo "Install Git:"
        echo "  Ubuntu/Debian: sudo apt-get install git"
        echo "  CentOS/RHEL: sudo yum install git"
        echo "  Fedora: sudo dnf install git"
    fi
fi

echo ""
echo "🎯 Dependency Check Complete!"
echo ""
echo "Next steps:"
echo "1. Fix any missing dependencies listed above"
echo "2. Build MoLab: ./tools/build_and_run.sh"
echo "3. Launch GUI: ./launch_gui.sh"
echo ""
echo "For help, see README.md or GUI_GUIDE.md"
