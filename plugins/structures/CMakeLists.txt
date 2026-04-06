# Standalone CMake for Structures plugin.
# This file allows building only this module outside the root build.

cmake_minimum_required(VERSION 3.20)
project(structures_plugin_standalone LANGUAGES CXX)

# Project C++ standard.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencies required by the plugin.
find_package(flatbuffers CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Repository root directory (this CMake lives in plugins/structures).
get_filename_component(MOLAB_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Allows overriding generated FlatBuffers headers directory from CLI.
set(MOLAB_GENERATED_DIR "" CACHE PATH "Directory containing state_vector_generated.h")

# Resolves FlatBuffers-generated include path.
set(_generated_candidates)
if(MOLAB_GENERATED_DIR)
    list(APPEND _generated_candidates "${MOLAB_GENERATED_DIR}")
endif()
list(APPEND _generated_candidates
    "${MOLAB_ROOT_DIR}/build/generated"
    "${MOLAB_ROOT_DIR}/out/build/generated"
    "${CMAKE_BINARY_DIR}/generated"
)

set(GENERATED_INCLUDE_DIR "")
foreach(_candidate IN LISTS _generated_candidates)
    if(EXISTS "${_candidate}/state_vector_generated.h")
        set(GENERATED_INCLUDE_DIR "${_candidate}")
        break()
    endif()
endforeach()

if(NOT GENERATED_INCLUDE_DIR)
    message(FATAL_ERROR
        "state_vector_generated.h was not found. "
        "Build the root project first or pass -DMOLAB_GENERATED_DIR=<path>/generated")
endif()

# Plugin library.
add_library(structures SHARED
    structures.cpp
    structures_module.cpp
)

# Include directories.
target_include_directories(structures PRIVATE
    "${MOLAB_ROOT_DIR}/src/api"
    "${MOLAB_ROOT_DIR}/plugins/structures"
    "${GENERATED_INCLUDE_DIR}"
)

# Link dependencies.
target_link_libraries(structures PRIVATE
    flatbuffers::flatbuffers
    nlohmann_json::nlohmann_json
)

# Compiler configuration.
target_compile_features(structures PRIVATE cxx_std_17)
target_compile_options(structures PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fPIC>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
)

# Output directories (simple layout for local plugin build).
set_target_properties(structures PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# Platform-specific naming.
if(WIN32)
    set_target_properties(structures PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(structures PROPERTIES
        PREFIX "lib"
        SUFFIX ".dylib"
    )
else()
    set_target_properties(structures PROPERTIES
        PREFIX "lib"
        SUFFIX ".so"
    )
endif()

# Plugin installation.
install(TARGETS structures
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)
