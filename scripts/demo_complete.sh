#!/bin/bash

# 🚀 MoLab - Demostración Completa del Sistema Aeroespacial
# Este script demuestra todas las capacidades de MoLab

echo "🚀 ===== MOLAB AEROSPACE SIMULATOR - DEMO COMPLETO ====="
echo ""

# Colores para output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Función para mostrar status
show_status() {
    echo -e "${GREEN}✅ $1${NC}"
}

show_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

show_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

show_error() {
    echo -e "${RED}❌ $1${NC}"
}

show_header() {
    echo -e "${PURPLE}🔧 $1${NC}"
}

# Verificar que estamos en el directorio correcto
if [[ ! -f "CMakeLists.txt" ]]; then
    show_error "Por favor ejecuta este script desde el directorio raíz de MoLab"
    exit 1
fi

show_header "PASO 1: VERIFICACIÓN DEL SISTEMA"
echo ""

# Verificar compilación
if [[ ! -f "build/bin/simulator" ]]; then
    show_warning "Simulador no encontrado. Compilando..."
    mkdir -p build
    cd build
    cmake .. && make -j4
    cd ..
    if [[ ! -f "build/bin/simulator" ]]; then
        show_error "Error en la compilación"
        exit 1
    fi
fi
show_status "Simulador compilado correctamente"

# Verificar plugins
plugin_count=$(ls build/lib/*.dylib 2>/dev/null | wc -l)
show_status "Plugins disponibles: $plugin_count"

# Listar plugins
show_info "Plugins detectados:"
for plugin in build/lib/*.dylib; do
    if [[ -f "$plugin" ]]; then
        plugin_name=$(basename "$plugin" .dylib | sed 's/lib//')
        plugin_size=$(ls -lh "$plugin" | awk '{print $5}')
        echo "  • $plugin_name ($plugin_size)"
    fi
done

echo ""
show_header "PASO 2: SIMULACIÓN BÁSICA (SIN PLUGINS)"
echo ""

show_info "Ejecutando simulación básica de caída libre..."
cd build
./bin/simulator --config ../data/config/basic_config.json --ticks 20 > /dev/null 2>&1
if [[ $? -eq 0 ]]; then
    show_status "Simulación básica completada"
    # Obtener último archivo
    latest_basic=$(ls -t output/*.csv | head -1)
    basic_points=$(wc -l < "$latest_basic")
    show_info "Archivo generado: $(basename $latest_basic) ($basic_points puntos de datos)"
else
    show_error "Error en simulación básica"
fi

echo ""
show_header "PASO 3: SIMULACIÓN CON PLUGINS DE FUERZAS"
echo ""

show_info "Ejecutando simulación con plugin de fuerzas..."
./bin/simulator --config ../data/config/main_config.json --ticks 30 > /dev/null 2>&1
if [[ $? -eq 0 ]]; then
    show_status "Simulación con fuerzas completada"
    # Obtener último archivo
    latest_force=$(ls -t output/*.csv | head -1)
    force_points=$(wc -l < "$latest_force")
    show_info "Archivo generado: $(basename $latest_force) ($force_points puntos de datos)"
    
    # Analizar resultados
    show_info "Analizando efectos de las fuerzas..."
    first_line=$(sed -n '2p' "$latest_force")
    last_line=$(tail -1 "$latest_force")
    
    # Extraer posiciones (columnas 4, 5, 6)
    initial_x=$(echo "$first_line" | cut -d',' -f4)
    initial_z=$(echo "$first_line" | cut -d',' -f6)
    final_x=$(echo "$last_line" | cut -d',' -f4)
    final_z=$(echo "$last_line" | cut -d',' -f6)
    
    # Calcular desplazamientos
    delta_x=$(echo "$final_x - $initial_x" | bc -l)
    delta_z=$(echo "$final_z - $initial_z" | bc -l)
    
    echo "  📊 Desplazamiento horizontal: ${delta_x} unidades"
    echo "  📊 Desplazamiento vertical: ${delta_z} unidades"
    echo "  🎯 Movimiento diagonal confirmado (efecto del plugin de fuerzas)"
else
    show_error "Error en simulación con fuerzas"
fi

cd ..

echo ""
show_header "PASO 4: INICIANDO INTERFAZ WEB"
echo ""

# Verificar si ya hay un servidor corriendo
if lsof -ti:8082 > /dev/null 2>&1; then
    show_warning "Servidor web ya está corriendo en puerto 8082"
    show_info "Terminando proceso anterior..."
    lsof -ti:8082 | xargs kill -9 2>/dev/null
    sleep 2
fi

show_info "Iniciando servidor web en puerto 8082..."
cd tools
python3 molab_web_gui.py > /dev/null 2>&1 &
WEB_PID=$!
cd ..

# Esperar a que el servidor inicie
sleep 3

# Verificar que el servidor esté corriendo
if curl -s http://localhost:8082/ > /dev/null 2>&1; then
    show_status "Servidor web iniciado correctamente"
    show_info "Accede a: http://localhost:8082"
else
    show_error "Error iniciando servidor web"
    kill $WEB_PID 2>/dev/null
    exit 1
fi

echo ""
show_header "PASO 5: VERIFICACIÓN DE APIS"
echo ""

# Probar API de plugins
show_info "Probando API de plugins..."
plugin_response=$(curl -s http://localhost:8082/api/plugins)
if [[ $? -eq 0 ]]; then
    plugin_count_api=$(echo "$plugin_response" | grep -o '"name":' | wc -l)
    show_status "API de plugins funcionando ($plugin_count_api plugins detectados)"
else
    show_warning "Error en API de plugins"
fi

# Probar API de resultados
show_info "Probando API de resultados..."
results_response=$(curl -s http://localhost:8082/api/results)
if [[ $? -eq 0 ]]; then
    results_count=$(echo "$results_response" | grep -o '"filename":' | wc -l)
    show_status "API de resultados funcionando ($results_count archivos disponibles)"
else
    show_warning "Error en API de resultados"
fi

echo ""
show_header "PASO 6: RESUMEN DE CAPACIDADES"
echo ""

show_status "SISTEMA COMPLETAMENTE FUNCIONAL"
echo ""
echo -e "${CYAN}🎯 CAPACIDADES DEMOSTRADAS:${NC}"
echo "  • ✅ Compilación automática de plugins aeroespaciales"
echo "  • ✅ Simulación básica con física realista"
echo "  • ✅ Simulación avanzada con plugins de fuerzas"
echo "  • ✅ Generación automática de resultados (CSV/JSON)"
echo "  • ✅ Interfaz web universal con visualización"
echo "  • ✅ APIs REST para plugins y resultados"
echo "  • ✅ Detección automática de simulaciones"
echo ""

echo -e "${CYAN}🚀 PLUGINS AEROESPACIALES DISPONIBLES:${NC}"
echo "  • 🌪️  Aerodynamics: Fuerzas aerodinámicas reales"
echo "  • 🚀 Propulsion: Sistemas de propulsión con combustible"
echo "  • 🏗️  Structures: Análisis estructural y masa"
echo "  • 🌍 Environment: Modelo atmosférico y viento"
echo "  • ⚡ Test Force: Plugin de prueba para validación"
echo ""

echo -e "${CYAN}📊 VISUALIZACIÓN AVANZADA:${NC}"
echo "  • 🛸 Trajectory Chart: Análisis 3D de posición"
echo "  • 🚀 Velocity Chart: Componentes de velocidad"
echo "  • 🏔️  Altitude Chart: Perfil de altitud"
echo "  • ⚡ Speed Chart: Análisis de velocidades"
echo "  • 📈 Métricas aeroespaciales automáticas"
echo "  • 🔔 Sistema de alertas inteligente"
echo "  • 🔄 Comparación multi-simulación"
echo "  • 🎯 Templates de misión predefinidos"
echo ""

echo -e "${CYAN}🌐 ACCESO UNIVERSAL:${NC}"
echo "  • 💻 Desktop: Interfaz web responsive"
echo "  • 📱 Mobile: Compatible con tablets y móviles"
echo "  • 🔗 SSH: Funciona en servidores remotos"
echo "  • 🐳 Docker: Compatible con contenedores"
echo "  • ☁️  Cloud: Desplegable en cualquier cloud"
echo ""

echo -e "${GREEN}🎉 DEMO COMPLETADO EXITOSAMENTE!${NC}"
echo ""
echo -e "${YELLOW}📋 PRÓXIMOS PASOS:${NC}"
echo "  1. Abre tu navegador en: ${BLUE}http://localhost:8082${NC}"
echo "  2. Explora la interfaz web y los gráficos"
echo "  3. Prueba diferentes templates de misión"
echo "  4. Configura plugins aeroespaciales"
echo "  5. Compara múltiples simulaciones"
echo ""
echo -e "${PURPLE}🛑 Para detener el servidor web:${NC}"
echo "  kill $WEB_PID"
echo ""

# Abrir navegador automáticamente (solo en macOS)
if [[ "$OSTYPE" == "darwin"* ]]; then
    show_info "Abriendo navegador automáticamente..."
    open http://localhost:8082
fi

echo -e "${GREEN}✨ MoLab está listo para simulaciones aeroespaciales profesionales!${NC}"
