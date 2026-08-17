# Minimum CMake version 3.20
cmake_minimum_required(VERSION 3.20)
project(Apogeo VERSION 0.0.0 LANGUAGES CXX)
include(FetchContent)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directory layout
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Dependencies
find_package(flatbuffers CONFIG REQUIRED) # FlatBuffers for data serialization/deserialization
find_package(nlohmann_json CONFIG REQUIRED) # nlohmann_json for JSON handling
find_package(Boost REQUIRED) # Boost (header-only, used by Odeint)

# Directories for FlatBuffers schemas and generated files
set(SCHEMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/schemas")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")

set(SCHEMA_FILE "${SCHEMA_DIR}/state_vector.fbs")
set(GENERATED_HEADER "${GENERATED_DIR}/state_vector_generated.h")

find_program(FLATC_EXECUTABLE flatc REQUIRED)

# Command to generate FlatBuffers files
add_custom_command(
  OUTPUT ${GENERATED_HEADER}
  COMMAND ${FLATC_EXECUTABLE} -c --gen-mutable -o ${GENERATED_DIR} ${SCHEMA_FILE}
  DEPENDS ${SCHEMA_FILE}
  COMMENT "Generating C++ sources from FlatBuffers schema..."
)

# Ensures generated headers are available for targets that require them.
add_custom_target(
  generate_flatbuffers ALL
  DEPENDS ${GENERATED_HEADER}
)

# --- Eigen (header-only, via FetchContent) ---
FetchContent_Declare(
    Eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    GIT_SHALLOW TRUE
)

FetchContent_GetProperties(Eigen)
if(NOT eigen_POPULATED)
    FetchContent_Populate(Eigen)
endif()

# Create header-only target manually (without add_subdirectory).
# This avoids Eigen registering tests, docs, or demos.
add_library(Eigen3::Eigen INTERFACE IMPORTED GLOBAL)
set_target_properties(Eigen3::Eigen PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${eigen_SOURCE_DIR}"
)

# --- Core configuration ---
set(CORE_SOURCES
    src/core/SimulationEngine.cpp
    src/core/PluginManager.cpp
    src/core/InitialStateLoader.cpp
    src/core/ConfigManager.cpp
    src/core/PhysicsIntegrator.cpp
    src/core/OutputManager.cpp
    src/core/TimeManager.cpp
)

# Create core library
add_library(core STATIC ${CORE_SOURCES})

# Configure include directories
target_include_directories(core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/api
    ${GENERATED_DIR}
)

# Link dependencies
target_link_libraries(core PUBLIC
    flatbuffers::flatbuffers
    nlohmann_json::nlohmann_json
    Eigen3::Eigen
)

target_include_directories(core PUBLIC
    ${Boost_INCLUDE_DIRS}
)

add_dependencies(core generate_flatbuffers)

# --- Main executable configuration ---
# Main executable
add_executable(simulator
    src/main.cpp
    src/ipc/IpcSession.cpp
)
target_link_libraries(simulator core ${CMAKE_DL_LIBS})
target_include_directories(simulator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Create required directories and copy runtime data files
add_custom_command(
    TARGET simulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/data"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/data/mass"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/data/limits"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/logs"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:simulator>/output"
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/data/defaults/default_state.json
            "$<TARGET_FILE_DIR:simulator>/data"
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/data/defaults/default_config.json
            "$<TARGET_FILE_DIR:simulator>/data"
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/data/mass/mass_properties.json
            "$<TARGET_FILE_DIR:simulator>/data/mass"
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/data/limits/structural_limits.csv
            "$<TARGET_FILE_DIR:simulator>/data/limits"
    COMMENT "Copying runtime configuration and data files..."
)

# --- Plugins ---

set(PLUGIN_NAMES
    structures
)

# Standardized plugin output layout:
# - Plugin runtime binaries (.dll/.so/.dylib) -> bin/plugins
# - Archive/import libraries (.lib/.a)        -> lib/plugins
set(PLUGIN_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
set(PLUGIN_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/plugins")

foreach(_plugin_name IN LISTS PLUGIN_NAMES)

    set(_plugin_sources
        plugins/${_plugin_name}/${_plugin_name}.cpp
    )

    set(_plugin_private_libs
        flatbuffers::flatbuffers
    )

    if(_plugin_name STREQUAL "structures")
        list(APPEND _plugin_sources
            "plugins/structures/structures_module.cpp"
        )
        list(APPEND _plugin_private_libs
            nlohmann_json::nlohmann_json
        )
    endif()

    add_library(${_plugin_name} SHARED ${_plugin_sources})
    add_dependencies(${_plugin_name} generate_flatbuffers)

    # Keep plugin binary output path consistent across platforms/configurations
    set_target_properties(${_plugin_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PLUGIN_RUNTIME_OUTPUT_DIRECTORY}"
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_RUNTIME_OUTPUT_DIRECTORY}"
        ARCHIVE_OUTPUT_DIRECTORY "${PLUGIN_ARCHIVE_OUTPUT_DIRECTORY}"
    )

    target_include_directories(${_plugin_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/api"
        "${GENERATED_DIR}"
    )

    if(_plugin_name STREQUAL "structures")
        target_include_directories(${_plugin_name} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/plugins/structures"
        )
    endif()

    target_link_libraries(${_plugin_name} PRIVATE ${_plugin_private_libs})

endforeach()

# --- Deprecated Plugins ---

set(DEPRECATED_PLUGIN_NAMES
    aerodynamics
    propulsion
    environment
)

# Standardized plugin output layout:
# - Plugin runtime binaries (.dll/.so/.dylib) -> bin/plugins
# - Archive/import libraries (.lib/.a)        -> lib/plugins
set(PLUGIN_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/plugins")
set(PLUGIN_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/plugins")

foreach(_plugin_name IN LISTS DEPRECATED_PLUGIN_NAMES)

    set(_plugin_sources
        plugins/deprecated/${_plugin_name}/${_plugin_name}.cpp
    )

    set(_plugin_private_libs
        flatbuffers::flatbuffers
    )

    add_library(${_plugin_name} SHARED ${_plugin_sources})
    add_dependencies(${_plugin_name} generate_flatbuffers)

    # Keep plugin binary output path consistent across platforms/configurations
    set_target_properties(${_plugin_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PLUGIN_RUNTIME_OUTPUT_DIRECTORY}"
        LIBRARY_OUTPUT_DIRECTORY "${PLUGIN_RUNTIME_OUTPUT_DIRECTORY}"
        ARCHIVE_OUTPUT_DIRECTORY "${PLUGIN_ARCHIVE_OUTPUT_DIRECTORY}"
    )

    target_include_directories(${_plugin_name} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/api"
        "${GENERATED_DIR}"
    )

    target_link_libraries(${_plugin_name} PRIVATE ${_plugin_private_libs})

endforeach()


message(STATUS "Configured plugins: ${PLUGIN_NAMES}")
message(STATUS "Plugin runtime output dir: ${PLUGIN_RUNTIME_OUTPUT_DIRECTORY}")

# --- Build options for coverage and analysis ---
option(ENABLE_COVERAGE "Enable code coverage" OFF)
option(ENABLE_TESTING "Enable unit tests" ON)

# Code coverage configuration
if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    else()
        message(WARNING "Code coverage only supported with GCC or Clang")
    endif()
endif()

# --- Unit tests ---
if(ENABLE_TESTING)
    enable_testing()

    # Find Google Test
    find_package(GTest CONFIG REQUIRED)

    # Core tests
    add_executable(core_tests
        tests/test_config_manager.cpp
        tests/test_physics_integrator.cpp
        tests/test_time_manager.cpp
        tests/test_logger.cpp
        tests/test_output_manager.cpp
        tests/test_simulation_engine.cpp
        tests/test_initial_state_loader.cpp
        tests/test_command_event_protocol.cpp
        tests/test_ipc_session.cpp
        src/ipc/IpcSession.cpp
    )

    target_link_libraries(core_tests PRIVATE
        core
        GTest::gtest
        GTest::gtest_main
    )

    target_include_directories(core_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/api
        ${GENERATED_DIR}
    )

    # Register tests with CTest
    include(GoogleTest)
    gtest_discover_tests(core_tests)

    message(STATUS "Unit tests enabled")
endif()
