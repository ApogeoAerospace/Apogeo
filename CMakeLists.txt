cmake_minimum_required(VERSION 3.20)
project(Molab VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(flatbuffers CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(SCHEMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/schemas")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")

set(SCHEMA_FILE "${SCHEMA_DIR}/state_vector.fbs")
set(GENERATED_HEADER "${GENERATED_DIR}/state_vector_generated.h")

find_program(FLATC_EXECUTABLE flatc REQUIRED)

add_custom_command(
  OUTPUT ${GENERATED_HEADER}
  COMMAND ${FLATC_EXECUTABLE} -c --gen-mutable -o ${GENERATED_DIR} ${SCHEMA_FILE}
  DEPENDS ${SCHEMA_FILE}
  COMMENT "Generating C++ sources from FlatBuffers schema..."
)

add_custom_target(
  generate_flatbuffers ALL
  DEPENDS ${GENERATED_HEADER}
)

# --- Librería del Núcleo (Core) ---
add_library(core STATIC
  src/core/PluginManager.cpp
  src/core/InitialStateGenerator.cpp
  src/core/SimulationEngine.cpp
)

add_dependencies(core generate_flatbuffers)

target_include_directories(core PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/api"
  "${GENERATED_DIR}"
)

target_link_libraries(core PUBLIC
  flatbuffers::flatbuffers
  nlohmann_json::nlohmann_json
)

# --- Ejecutable Principal ---
add_executable(simulator
  src/main.cpp
)

add_custom_command(
    TARGET simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/data"

    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/data/initial_state/initial_state.json"
            "$<TARGET_FILE_DIR:simulator>/data"
    COMMENT "Copiando archivo de estado inicial a la carpeta data..."
)

target_link_libraries(simulator PRIVATE core)

add_dependencies(simulator example_plugin)

# --- Plugins ---
add_library(example_plugin SHARED
  plugins/example_plugin/plugin.cpp
)

add_dependencies(example_plugin generate_flatbuffers)

target_include_directories(example_plugin PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src/api"
  "${GENERATED_DIR}"
)

target_link_libraries(example_plugin PRIVATE flatbuffers::flatbuffers)
