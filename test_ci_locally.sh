#!/bin/bash

# Script para probar el CI pipeline localmente
# Simula los pasos del GitHub Actions workflow

set -e  # Exit on error

echo "🚀 MoLab CI Local Test"
echo "======================"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print status
print_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✅ $2${NC}"
    else
        echo -e "${RED}❌ $2${NC}"
        exit 1
    fi
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# 1. Code Quality Checks
echo "📋 Step 1: Code Quality Checks"
echo "--------------------------------"

# Check for trailing whitespace
echo "Checking for trailing whitespace..."
if git grep -I -n '[[:space:]]$' -- '*.cpp' '*.h' '*.hpp' 2>/dev/null; then
    print_warning "Found trailing whitespace (non-critical)"
else
    print_status 0 "No trailing whitespace"
fi

# Validate JSON files
echo "Validating JSON files..."
JSON_VALID=0
for json_file in $(find data -name '*.json' -type f); do
    if python3 -m json.tool "$json_file" > /dev/null 2>&1; then
        echo "  ✓ $json_file"
    else
        echo "  ✗ $json_file"
        JSON_VALID=1
    fi
done
print_status $JSON_VALID "JSON validation"

# Validate Python syntax
echo "Validating Python scripts..."
PYTHON_VALID=0
for py_file in tools/*.py; do
    if [ -f "$py_file" ]; then
        if python3 -m py_compile "$py_file" 2>/dev/null; then
            echo "  ✓ $py_file"
        else
            echo "  ✗ $py_file"
            PYTHON_VALID=1
        fi
    fi
done
print_status $PYTHON_VALID "Python syntax validation"

echo ""

# 2. Build and Compile
echo "🔨 Step 2: Build and Compile"
echo "-----------------------------"

# Clean previous build
if [ -d "build" ]; then
    echo "Cleaning previous build..."
    rm -rf build
fi

# Configure with CMake
echo "Configuring CMake..."

# Detectar si existe build previo
if [ -d "build" ]; then
    echo "Using existing build directory..."
    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -G Ninja 2>&1 | tee build_config.log
else
    # Intentar con vcpkg si existe
    if [ -f "./vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
        cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
            -G Ninja 2>&1 | tee build_config.log
    else
        echo "⚠️  vcpkg toolchain not found, using system dependencies"
        cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Release \
            -G Ninja 2>&1 | tee build_config.log
    fi
fi

CMAKE_RESULT=$?
if [ $CMAKE_RESULT -ne 0 ]; then
    echo ""
    echo "❌ CMake configuration failed!"
    echo "Possible solutions:"
    echo "  1. Install vcpkg: git clone https://github.com/Microsoft/vcpkg.git"
    echo "  2. Run: ./vcpkg/bootstrap-vcpkg.sh"
    echo "  3. Or use existing build: ./install_dependencies.sh"
    exit 1
fi

print_status $CMAKE_RESULT "CMake configuration"

# Build
echo "Building project..."
cmake --build build --config Release --parallel 2>&1 | tee build_compile.log
print_status $? "Project compilation"

# Verify binaries
echo "Verifying binaries..."
if [ -f "build/bin/simulator" ]; then
    echo "  ✓ Simulator binary: $(ls -lh build/bin/simulator | awk '{print $5}')"
else
    print_status 1 "Simulator binary not found"
fi

PLUGIN_COUNT=$(find build/lib -name "*.so" -o -name "*.dylib" 2>/dev/null | wc -l)
echo "  ✓ Plugins compiled: $PLUGIN_COUNT"

echo ""

# 3. Unit Tests
echo "🧪 Step 3: Unit Tests"
echo "---------------------"

# Reconfigure with testing enabled
echo "Configuring for testing..."

# Intentar con vcpkg si existe
if [ -f "./vcpkg/scripts/buildsystems/vcpkg.cmake" ]; then
    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DENABLE_TESTING=ON \
        -G Ninja > /dev/null 2>&1
else
    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Debug \
        -DENABLE_TESTING=ON \
        -G Ninja > /dev/null 2>&1
fi

print_status $? "Test configuration"

# Build tests
echo "Building tests..."
cmake --build build --config Debug --parallel > /dev/null 2>&1
print_status $? "Test compilation"

# Run tests
echo "Running unit tests..."
cd build
ctest --output-on-failure --verbose 2>&1 | tee ../test_results.log
TEST_RESULT=$?
cd ..
print_status $TEST_RESULT "Unit tests execution"

echo ""

# 4. Integration Test
echo "🔗 Step 4: Integration Test"
echo "----------------------------"

# Prepare environment
echo "Preparing test environment..."
mkdir -p build/bin/data/config
mkdir -p build/bin/data/initial_state
cp -r data/config/* build/bin/data/config/
cp -r data/initial_state/* build/bin/data/initial_state/

# Run simulation
echo "Running integration test simulation..."
cd build/bin
./simulator --config data/config/basic_config.json --ticks 10 2>&1 | tee ../../integration_test.log
INTEGRATION_RESULT=$?
cd ../..

print_status $INTEGRATION_RESULT "Integration test"

# Verify output
if [ -d "build/bin/output" ] && [ "$(ls -A build/bin/output 2>/dev/null)" ]; then
    echo "  ✓ Output files generated: $(ls build/bin/output | wc -l) files"
else
    print_warning "No output files generated"
fi

echo ""

# 5. Summary
echo "📊 Summary"
echo "----------"
echo "Build logs: build_config.log, build_compile.log"
echo "Test logs: test_results.log, integration_test.log"
echo ""
echo -e "${GREEN}✅ All CI checks passed locally!${NC}"
echo ""
echo "Next steps:"
echo "  1. Review logs if any warnings"
echo "  2. Commit and push changes"
echo "  3. Monitor GitHub Actions workflow"
echo ""
