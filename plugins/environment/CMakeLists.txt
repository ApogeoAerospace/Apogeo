# Standalone CMake for the Environment plugin.

cmake_minimum_required(VERSION 3.20)
project(environment_plugin_standalone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(flatbuffers CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

get_filename_component(MOLAB_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(MOLAB_GENERATED_DIR "" CACHE PATH "Directory containing state_vector_generated.h")

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
    message(FATAL_ERROR "state_vector_generated.h was not found. Build the root project first or pass -DMOLAB_GENERATED_DIR=<path>/generated")
endif()

add_library(environment SHARED
    environment.cpp
    environment_module.cpp
)

target_include_directories(environment PRIVATE
    "${MOLAB_ROOT_DIR}/src/api"
    "${MOLAB_ROOT_DIR}/plugins/environment"
    "${GENERATED_INCLUDE_DIR}"
)

target_link_libraries(environment PRIVATE
    flatbuffers::flatbuffers
    nlohmann_json::nlohmann_json
)

target_compile_features(environment PRIVATE cxx_std_17)
