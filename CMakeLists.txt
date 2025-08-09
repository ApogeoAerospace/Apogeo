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

# --- Configuración del núcleo ---
set(CORE_SOURCES
    core/SimulationEngine.cpp
    core/PluginManager.cpp
    core/InitialStateLoader.cpp
    src/core/ConfigManager.cpp
    src/core/PhysicsIntegrator.cpp
    src/core/OutputManager.cpp
    src/core/TimeManager.cpp
)

# Crear la biblioteca del núcleo
add_library(core STATIC ${CORE_SOURCES})

# Configurar includes
target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
    ${GENERATED_DIR}
)

# Vincular dependencias
target_link_libraries(core PUBLIC
    flatbuffers::flatbuffers
    nlohmann_json::nlohmann_json
)

add_dependencies(core generate_flatbuffers)

# --- Configuración del ejecutable principal ---
# Main executable
add_executable(simulator src/main.cpp)
target_link_libraries(simulator core ${CMAKE_DL_LIBS})
target_include_directories(simulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Crear directorios necesarios y copiar archivos de datos
add_custom_command(
    TARGET simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/data"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/logs"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/output"
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/data/initial_state/initial_state.json
            "$<TARGET_FILE_DIR:simulator>/data"
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/data/config/default_config.json
            "$<TARGET_FILE_DIR:simulator>/data"
    COMMENT "Copiando archivos de configuración y creando directorios..."
)

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
