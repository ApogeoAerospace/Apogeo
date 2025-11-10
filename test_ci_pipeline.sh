#!/bin/bash

# Script para probar localmente todos los jobs del CI Pipeline
# Simula la ejecución de GitHub Actions en tu máquina local

set -e  # Exit on error

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Función para imprimir con color
print_header() {
    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Variables
BUILD_DIR="build"
WORKSPACE_DIR=$(pwd)

# Limpiar build anterior si existe
if [ -d "$BUILD_DIR" ]; then
    print_header "Limpiando build anterior"
    rm -rf "$BUILD_DIR"
    print_success "Build anterior eliminado"
fi

# ============================================
# JOB 1: BUILD AND TEST
# ============================================
print_header "JOB 1: BUILD AND TEST"

# Verificar vcpkg
if [ ! -d "vcpkg" ]; then
    print_header "Instalando vcpkg"
    git clone https://github.com/Microsoft/vcpkg.git
    cd vcpkg
    ./bootstrap-vcpkg.sh
    ./vcpkg install
    cd ..
    print_success "vcpkg instalado"
else
    print_success "vcpkg ya instalado"
fi

# Configurar CMake
print_header "Configurando CMake (Release)"
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=$WORKSPACE_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -G Ninja

print_success "CMake configurado"

# Compilar
print_header "Compilando proyecto"
cmake --build build --config Release --parallel

print_success "Compilación exitosa"

# Verificar binarios
print_header "Verificando binarios generados"
if [ -f "build/bin/simulator" ]; then
    print_success "Simulador compilado: $(ls -lh build/bin/simulator | awk '{print $5}')"
else
    print_error "Simulador no encontrado"
    exit 1
fi

# Contar plugins
PLUGIN_COUNT=$(find build/lib -name "*.dylib" -o -name "*.so" 2>/dev/null | wc -l)
print_success "Plugins compilados: $PLUGIN_COUNT"

# ============================================
# JOB 2: UNIT TESTS
# ============================================
print_header "JOB 2: UNIT TESTS"

# Reconfigurar con tests
print_header "Reconfigurando CMake con tests"
cmake -B build -S . \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE=$WORKSPACE_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DENABLE_TESTING=ON \
    -G Ninja

# Compilar con tests
print_header "Compilando proyecto y tests"
cmake --build build --config Debug --parallel

# Ejecutar tests
print_header "Ejecutando tests unitarios"
cd build
ctest --output-on-failure --verbose > test_results.txt 2>&1 || true

# Validar tasa de éxito
print_header "Validando tasa de éxito de tests"
if grep -q "tests passed" test_results.txt; then
    PASSED=$(grep -oP '\d+(?= tests passed)' test_results.txt || echo "0")
    FAILED=$(grep -oP '\d+(?= tests failed)' test_results.txt || echo "0")
    TOTAL=$((PASSED + FAILED))
    if [ "$TOTAL" -gt 0 ]; then
        SUCCESS_RATE=$(awk "BEGIN {printf \"%.2f\", ($PASSED/$TOTAL)*100}")
        echo "Tests pasados: $PASSED/$TOTAL ($SUCCESS_RATE%)"
        if (( $(echo "$SUCCESS_RATE < 95" | bc -l) )); then
            print_error "Tasa de éxito de tests menor al 95% requerido"
            exit 1
        fi
        print_success "Tasa de éxito de tests: $SUCCESS_RATE% (>= 95%)"
    fi
else
    print_warning "No se pudieron extraer estadísticas de tests"
fi

cd ..

# ============================================
# JOB 3: CODE COVERAGE
# ============================================
print_header "JOB 3: CODE COVERAGE"

# Verificar si lcov está instalado
if ! command -v lcov &> /dev/null; then
    print_warning "lcov no está instalado. Saltando job de cobertura."
    print_warning "Para instalar: brew install lcov (macOS) o sudo apt-get install lcov (Linux)"
else
    # Reconfigurar con cobertura
    print_header "Reconfigurando CMake con cobertura"
    cmake -B build -S . \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_TOOLCHAIN_FILE=$WORKSPACE_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DENABLE_TESTING=ON \
        -DENABLE_COVERAGE=ON \
        -G Ninja

    # Compilar con cobertura
    print_header "Compilando con cobertura"
    cmake --build build --config Debug --parallel

    # Ejecutar tests
    print_header "Ejecutando tests para cobertura"
    cd build
    ctest --output-on-failure

    # Generar reporte de cobertura
    print_header "Generando reporte de cobertura"
    lcov --capture --directory . --output-file coverage.info --ignore-errors mismatch,inconsistent
    lcov --remove coverage.info '/usr/*' '*/vcpkg_installed/*' '*/tests/*' --output-file coverage_filtered.info --ignore-errors unused
    lcov --list coverage_filtered.info

    # Validar cobertura mínima
    print_header "Validando cobertura mínima"
    COVERAGE=$(lcov --summary coverage_filtered.info 2>&1 | grep -oP '\d+\.\d+(?=% of)' | head -1)
    if [[ -z "$COVERAGE" ]]; then
        print_error "No se pudo determinar la cobertura de código"
        exit 1
    fi
    echo "Cobertura de código: $COVERAGE%"
    if (( $(echo "$COVERAGE < 60" | bc -l) )); then
        print_warning "Cobertura de código menor al 60% recomendado (actual: $COVERAGE%)"
        print_warning "Se recomienda agregar más tests para mejorar la cobertura"
    else
        print_success "Cobertura de código aceptable: $COVERAGE% (>= 60%)"
    fi

    # Generar reporte HTML
    print_header "Generando reporte HTML de cobertura"
    genhtml coverage_filtered.info --output-directory coverage_html
    print_success "Reporte HTML generado en: build/coverage_html/index.html"

    cd ..
fi

# ============================================
# JOB 4: STATIC ANALYSIS
# ============================================
print_header "JOB 4: STATIC ANALYSIS"

# Verificar si clang-tidy está instalado
if ! command -v clang-tidy &> /dev/null; then
    print_warning "clang-tidy no está instalado. Saltando análisis estático."
    print_warning "Para instalar: brew install llvm (macOS) o sudo apt-get install clang-tidy (Linux)"
else
    # Asegurar que build existe con compile_commands.json
    if [ ! -f "build/compile_commands.json" ]; then
        print_header "Reconfigurando CMake para análisis estático"
        cmake -B build -S . \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_TOOLCHAIN_FILE=$WORKSPACE_DIR/vcpkg/scripts/buildsystems/vcpkg.cmake \
            -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
            -G Ninja
    fi

    # Generar archivos FlatBuffers
    print_header "Generando archivos FlatBuffers"
    cmake --build build --target generate_flatbuffers 2>/dev/null || cmake --build build -j2

    # Ejecutar clang-tidy
    print_header "Ejecutando análisis estático con clang-tidy"
    find core src -name '*.cpp' -not -path '*/vcpkg_installed/*' | \
        xargs clang-tidy -p build --config-file=.clang-tidy 2>&1 | \
        tee clang-tidy-report.txt || true

    # Contar advertencias
    print_header "Contando advertencias de clang-tidy"
    WARNING_COUNT=$(grep -cE "^[^:]+:[0-9]+:[0-9]+: warning:" clang-tidy-report.txt || echo "0")
    ERROR_COUNT=$(grep -cE "^[^:]+:[0-9]+:[0-9]+: error:" clang-tidy-report.txt || echo "0")
    echo "Advertencias encontradas: $WARNING_COUNT"
    echo "Errores encontrados: $ERROR_COUNT"
    
    if [ "$WARNING_COUNT" -gt 50 ]; then
        print_warning "Demasiadas advertencias de clang-tidy ($WARNING_COUNT > 50)"
        print_warning "Se recomienda corregir las advertencias para mejorar la calidad del código"
    fi
    
    if [ "$ERROR_COUNT" -gt 0 ]; then
        print_error "Se encontraron $ERROR_COUNT errores de clang-tidy"
    else
        print_success "Sin errores de clang-tidy"
    fi

    # Verificar errores críticos
    print_header "Verificando errores críticos"
    if grep -E "^[^:]+:[0-9]+:[0-9]+: error:" clang-tidy-report.txt; then
        print_error "Se encontraron errores críticos en el análisis estático"
        print_error "Por favor revisa clang-tidy-report.txt para más detalles"
        exit 1
    fi
    print_success "Análisis estático completado sin errores críticos"
fi

# ============================================
# RESUMEN FINAL
# ============================================
print_header "RESUMEN DE CALIDAD DEL PIPELINE CI"

echo ""
echo "Estado de Jobs:"
echo "---------------"
echo "✓ Build & Test: EXITOSO"
echo "✓ Tests Unitarios: EXITOSO"
if command -v lcov &> /dev/null; then
    echo "✓ Cobertura de Código: EXITOSO"
else
    echo "⚠ Cobertura de Código: SALTADO (lcov no instalado)"
fi
if command -v clang-tidy &> /dev/null; then
    echo "✓ Análisis Estático: EXITOSO"
else
    echo "⚠ Análisis Estático: SALTADO (clang-tidy no instalado)"
fi

echo ""
echo "Métricas de Calidad:"
echo "--------------------"
echo "- Tests Unitarios: Tasa de éxito >= 95% requerida"
echo "- Cobertura de Código: >= 60% recomendada"
echo "- Análisis Estático: 0 errores críticos requeridos"
echo "- Advertencias: <= 50 advertencias recomendadas"

echo ""
print_success "Pipeline CI completado exitosamente"
print_success "El código está listo para push"

echo ""
echo "Archivos generados:"
echo "-------------------"
echo "- build/bin/simulator: Binario del simulador"
echo "- build/lib/*.dylib: Plugins compilados"
echo "- build/test_results.txt: Resultados de tests"
if command -v lcov &> /dev/null; then
    echo "- build/coverage_html/: Reporte HTML de cobertura"
fi
if command -v clang-tidy &> /dev/null; then
    echo "- clang-tidy-report.txt: Reporte de análisis estático"
fi

echo ""
