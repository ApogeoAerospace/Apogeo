# Guía de Instalación de MoLab

## Requisitos del Sistema

### **Sistemas Operativos Soportados**
- **Windows 10/11** (64-bit)
- **macOS 10.15+** (Catalina o superior)
- **Linux** (Ubuntu 18.04+, CentOS 7+, Fedora 30+)

### **Herramientas Requeridas**

#### **Compilador C++**
- **Windows**: Visual Studio 2019+ o Visual Studio Build Tools
- **macOS**: Xcode Command Line Tools
- **Linux**: GCC 7+ o Clang 5+

#### **Sistema de Compilación**
- **CMake 3.15+**
- **Make** (Linux/macOS) o **MSBuild** (Windows)

#### **Dependencias**
- **FlatBuffers** (incluido como submódulo)
- **nlohmann/json** (incluido como submódulo)
- **Python 3.7+** (para interfaces web y GUI)

## Instalación Paso a Paso

### **Paso 1: Clonar el Repositorio**

```bash
# Clonar con submódulos
git clone --recursive https://github.com/Samuelbomc/MoLab.git
cd MoLab

# Si ya clonaste sin submódulos, ejecuta:
git submodule update --init --recursive
```

### **Paso 2: Configurar Dependencias**

#### **Windows**
```powershell
# Instalar vcpkg (si no está instalado)
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Instalar dependencias
.\vcpkg install flatbuffers:x64-windows
.\vcpkg install nlohmann-json:x64-windows

# Volver al directorio de MoLab
cd ..\MoLab
```

#### **macOS**
```bash
# Instalar Homebrew (si no está instalado)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Instalar dependencias
brew install cmake
brew install flatbuffers
brew install nlohmann-json
brew install python3

# Instalar Xcode Command Line Tools
xcode-select --install
```

#### **Linux (Ubuntu/Debian)**
```bash
# Actualizar paquetes
sudo apt update

# Instalar herramientas de compilación
sudo apt install build-essential cmake git

# Instalar dependencias
sudo apt install libflatbuffers-dev
sudo apt install nlohmann-json3-dev
sudo apt install python3 python3-pip

# Para otras distribuciones, usar el gestor de paquetes correspondiente
```

### **Paso 3: Compilar MoLab**

#### **Compilación Estándar**
```bash
# Crear directorio de compilación
mkdir build
cd build

# Configurar con CMake
cmake ..

# Compilar (ajustar -j según número de cores)
make -j4

# En Windows con Visual Studio:
# cmake --build . --config Release
```

#### **Compilación con Opciones Avanzadas**
```bash
# Configuración con opciones específicas
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=ON \
    -DENABLE_PLUGINS=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local

# Compilar con verbosidad
make -j4 VERBOSE=1
```

### **Paso 4: Verificar la Instalación**

```bash
# Verificar que el simulador se compiló correctamente
ls -la build/bin/simulator

# Verificar plugins compilados
ls -la build/lib/

# Ejecutar prueba básica
cd build
./bin/simulator --help
```

## Configuración del Entorno

### **Dependencias de Python (Interfaces)**

```bash
# Instalar dependencias para interfaces
pip3 install flask
pip3 install tkinter  # Puede venir preinstalado
pip3 install matplotlib  # Para análisis de resultados

# O usar requirements.txt si está disponible
pip3 install -r requirements.txt
```

### **Configuración de Red (Interfaz Web)**

```bash
# Verificar que el puerto 8082 esté disponible
netstat -an | grep 8082

# En macOS, permitir conexiones entrantes si es necesario
sudo pfctl -f /etc/pf.conf
```

### **Estructura de Directorios**

Después de la compilación exitosa, deberías tener:

```
MoLab/
├── build/
│   ├── bin/
│   │   └── simulator              # Ejecutable principal
│   ├── lib/
│   │   ├── libaerodynamics.dylib  # Plugins compilados
│   │   ├── libpropulsion.dylib
│   │   ├── libstructures.dylib
│   │   └── libenvironment.dylib
│   └── generated/
│       └── state_vector_generated.h
├── data/
│   └── config/                    # Configuraciones de ejemplo
├── tools/
│   ├── molab_web_gui.py          # Interfaz web
│   ├── molab_gui.py              # Interfaz desktop
│   └── analyze_results.py        # Análisis de resultados
└── output/                       # Resultados de simulación
```

## Configuración Inicial

### **Crear Configuración Básica**

```bash
# Copiar configuración de ejemplo
cp data/config/default_config.json data/config/mi_config.json

# Editar configuración según necesidades
nano data/config/mi_config.json
```

### **Primera Simulación de Prueba**

```bash
# Ejecutar simulación básica
cd build
./bin/simulator \
    --config ../data/config/default_config.json \
    --ticks 100 \
    --log-level INFO

# Verificar resultados
ls -la output/
```

### **Lanzar Interfaz Web**

```bash
# Desde el directorio raíz de MoLab
python3 tools/molab_web_gui.py

# La interfaz se abrirá automáticamente en http://localhost:8082
```

## Solución de Problemas Comunes

### **Errores de Compilación**

#### **Error: CMake no encuentra FlatBuffers**
```bash
# Solución: Instalar FlatBuffers manualmente
git clone https://github.com/google/flatbuffers.git
cd flatbuffers
cmake -G "Unix Makefiles"
make -j4
sudo make install
```

#### **Error: Compilador C++17 no soportado**
```bash
# Linux: Actualizar GCC
sudo apt install gcc-9 g++-9
export CC=gcc-9
export CXX=g++-9

# macOS: Actualizar Xcode
xcode-select --install
```

#### **Error: Símbolos no encontrados en plugins**
```bash
# Verificar que los plugins se compilaron correctamente
nm build/lib/libaerodynamics.dylib | grep plugin_

# Recompilar plugins específicos
cd build
make aerodynamics
```

### **Problemas de Ejecución**

#### **Error: Plugin no se puede cargar**
```bash
# Verificar dependencias de biblioteca
ldd build/lib/libaerodynamics.so  # Linux
otool -L build/lib/libaerodynamics.dylib  # macOS

# Verificar permisos
chmod +x build/lib/*.so
chmod +x build/lib/*.dylib
```

#### **Error: Puerto 8082 ocupado**
```bash
# Encontrar proceso usando el puerto
lsof -i :8082

# Cambiar puerto en la configuración
export MOLAB_WEB_PORT=8083
python3 tools/molab_web_gui.py
```

#### **Error: Archivos de configuración no encontrados**
```bash
# Verificar rutas relativas
pwd
ls -la data/config/

# Usar rutas absolutas
./bin/simulator --config /ruta/completa/a/config.json
```

### **Problemas de Python**

#### **Error: Módulo no encontrado**
```bash
# Verificar instalación de Python
python3 --version
pip3 --version

# Instalar módulos faltantes
pip3 install flask tkinter matplotlib
```

#### **Error: Tkinter no disponible**
```bash
# Ubuntu/Debian
sudo apt install python3-tk

# CentOS/RHEL
sudo yum install tkinter

# macOS (con Homebrew)
brew install python-tk
```

## Instalación para Desarrollo

### **Configuración de Desarrollo**

```bash
# Compilación con símbolos de debug
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON

# Compilar con sanitizers (opcional)
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address -fsanitize=undefined"

# Habilitar todas las advertencias
cmake .. -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic"
```

### **Configurar Pruebas**

```bash
# Compilar con pruebas habilitadas
cmake .. -DBUILD_TESTING=ON
make -j4

# Ejecutar suite de pruebas
ctest --verbose

# Ejecutar pruebas específicas
./build/tests/test_plugin_manager
```

### **Herramientas de Profiling**

```bash
# Instalar herramientas de profiling
# Linux
sudo apt install valgrind gprof

# macOS
brew install valgrind

# Ejecutar con Valgrind
valgrind --tool=memcheck ./build/bin/simulator --config config.json
```

## Instalación en Contenedores

### **Docker**

```dockerfile
# Dockerfile para MoLab
FROM ubuntu:20.04

# Instalar dependencias
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libflatbuffers-dev \
    nlohmann-json3-dev \
    python3 \
    python3-pip

# Copiar código fuente
COPY . /molab
WORKDIR /molab

# Compilar
RUN mkdir build && cd build && \
    cmake .. && \
    make -j4

# Instalar dependencias Python
RUN pip3 install flask matplotlib

# Exponer puerto para interfaz web
EXPOSE 8082

# Comando por defecto
CMD ["python3", "tools/molab_web_gui.py"]
```

```bash
# Construir imagen Docker
docker build -t molab:latest .

# Ejecutar contenedor
docker run -p 8082:8082 -v $(pwd)/output:/molab/output molab:latest
```

## Verificación de Instalación Completa

### **Lista de Verificación**

```bash
# 1. Verificar compilación del simulador
test -f build/bin/simulator && echo "✅ Simulador compilado"

# 2. Verificar plugins
ls build/lib/lib*.{so,dylib} 2>/dev/null | wc -l | \
    awk '{if($1>=4) print "✅ Plugins compilados ("$1")"; else print "❌ Plugins faltantes"}'

# 3. Verificar Python
python3 -c "import flask, tkinter; print('✅ Dependencias Python OK')" 2>/dev/null || \
    echo "❌ Dependencias Python faltantes"

# 4. Verificar configuraciones
test -f data/config/default_config.json && echo "✅ Configuraciones disponibles"

# 5. Prueba de ejecución rápida
cd build && timeout 10s ./bin/simulator --help >/dev/null 2>&1 && \
    echo "✅ Simulador ejecutable" || echo "❌ Error en simulador"
```

### **Script de Verificación Automática**

```bash
#!/bin/bash
# verify_installation.sh

echo "🔍 Verificando instalación de MoLab..."

# Verificar estructura de directorios
REQUIRED_DIRS=("build" "data" "tools" "plugins")
for dir in "${REQUIRED_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        echo "✅ Directorio $dir existe"
    else
        echo "❌ Directorio $dir faltante"
        exit 1
    fi
done

# Verificar ejecutables
if [ -f "build/bin/simulator" ]; then
    echo "✅ Simulador compilado correctamente"
else
    echo "❌ Simulador no encontrado"
    exit 1
fi

# Verificar plugins
PLUGIN_COUNT=$(ls build/lib/lib*.{so,dylib} 2>/dev/null | wc -l)
if [ "$PLUGIN_COUNT" -ge 4 ]; then
    echo "✅ Plugins compilados ($PLUGIN_COUNT encontrados)"
else
    echo "⚠️  Solo $PLUGIN_COUNT plugins encontrados (se esperan 4+)"
fi

# Verificar dependencias Python
python3 -c "import flask, tkinter" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✅ Dependencias Python disponibles"
else
    echo "⚠️  Algunas dependencias Python faltantes"
fi

# Prueba de ejecución
cd build
timeout 5s ./bin/simulator --help >/dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✅ Simulador ejecuta correctamente"
else
    echo "❌ Error al ejecutar simulador"
    exit 1
fi

echo ""
echo " Instalación de MoLab verificada exitosamente!"
echo " Consulta la documentación en docs/ para comenzar"
echo " Ejecuta 'python3 tools/molab_web_gui.py' para la interfaz web"
```

## Siguientes Pasos

Una vez completada la instalación:

1. **Lee la [Guía de Usuario](./06-guia-usuario.md)** para aprender a usar las interfaces
2. **Explora los [Ejemplos](./ejemplos/)** para configuraciones de muestra
3. **Consulta la [Documentación de Plugins](./plugins/)** para entender los módulos disponibles
4. **Revisa la [Guía de Configuración](./07-configuracion.md)** para personalizar simulaciones

¡MoLab está listo para simular misiones aeroespaciales realistas!
