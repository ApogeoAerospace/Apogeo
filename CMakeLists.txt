# CMake versión 3.20 mínima
cmake_minimum_required(VERSION 3.20)
project(Molab VERSION 0.0.0 LANGUAGES CXX)

# C++ 17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Organización del output
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Dependencias
find_package(flatbuffers CONFIG REQUIRED) # FlatBuffers para serialización y deserialización de datos
find_package(nlohmann_json CONFIG REQUIRED) # nlohmann_json para manejo de JSON

# Directorios con los esquemas de FlatBuffers y archivos generados
set(SCHEMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/schemas")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")

set(SCHEMA_FILE "${SCHEMA_DIR}/state_vector.fbs")
set(GENERATED_HEADER "${GENERATED_DIR}/state_vector_generated.h")

find_program(FLATC_EXECUTABLE flatc REQUIRED)

# Comando para generar los archivos de FlatBuffers
add_custom_command(
  OUTPUT ${GENERATED_HEADER}
  COMMAND ${FLATC_EXECUTABLE} -c --gen-mutable -o ${GENERATED_DIR} ${SCHEMA_FILE}
  DEPENDS ${SCHEMA_FILE}
  COMMENT "Generating C++ sources from FlatBuffers schema..."
)

# Se asegura de que los encabezados generados estén disponibles para los targets que los requieren.
add_custom_target(
  generate_flatbuffers ALL
  DEPENDS ${GENERATED_HEADER}
)

# --- Librería del Núcleo ---
# Se crea la librería estática del núcleo del simulador
add_library(core STATIC
  core/PluginManager.cpp # Clase para manejar plugins
  core/InitialStateLoader.cpp # Clase para cargar el estado inicial del simulador
  core/SimulationEngine.cpp # Clase para manejar la lógica del simulador
)

# Core requiere el schema de FlatBuffers generado
add_dependencies(core generate_flatbuffers)

# Se incluyen los directorios necesarios para el núcleo
target_include_directories(core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/api"
  "${GENERATED_DIR}"
)

# Se incluyen las librerías necesarias para el núcleo
target_link_libraries(core PUBLIC
  flatbuffers::flatbuffers
  nlohmann_json::nlohmann_json
)

# --- Ejecutable Principal ---

# Se crea el ejecutable principal del simulador
add_executable(simulator
  src/main.cpp
)

# Crea la carpeta 'data' junto al ejecutable y copia el archivo de estado inicial necesario para la simulación.
add_custom_command(
    TARGET simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/data"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/data/initial_state/initial_state.json"
            "$<TARGET_FILE_DIR:simulator>/data"
    COMMENT "Copiando archivo de estado inicial a la carpeta data..."
)

# Se incluyen los directorios necesarios para el ejecutable
target_link_libraries(simulator PRIVATE core)

# Este comando asegura que el programa principal necesite la copilación del plugin
add_dependencies(simulator example_plugin)

# --- Plugins ---

set(PLUGIN_NAMES
    example_plugin
    aerodynamics
    propulsion
    environment
    structures
    programming
)

foreach(_plugin_name IN LISTS PLUGIN_NAMES)

    # Crea la librería compartida
    add_library(${_plugin_name} SHARED
        plugins/${_plugin_name}/${_plugin_name}.cpp
    )

    # Establece las dependencias y directorios de inclusión
    add_dependencies(${_plugin_name} generate_flatbuffers)

    target_include_directories(${_plugin_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/api"
        "${GENERATED_DIR}"
    )

    # Enlaza las librerías necesarias
    target_link_libraries(${_plugin_name} PRIVATE flatbuffers::flatbuffers)

endforeach()

message(STATUS "Plugins configurados: ${PLUGIN_NAMES}")
