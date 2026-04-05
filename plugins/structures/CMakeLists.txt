# CMake standalone del plugin Structures.
# Este archivo permite compilar únicamente este módulo fuera del build raíz.

cmake_minimum_required(VERSION 3.20)
project(structures_plugin_standalone LANGUAGES CXX)

# Estándar C++ del proyecto.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Dependencias requeridas por el plugin.
find_package(flatbuffers CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Directorio raíz del repositorio (este CMake vive en plugins/structures).
get_filename_component(MOLAB_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

# Permite sobreescribir desde CLI el directorio de headers generados por FlatBuffers.
set(MOLAB_GENERATED_DIR "" CACHE PATH "Directorio que contiene state_vector_generated.h")

# Resuelve el include generado por FlatBuffers.
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
        "No se encontró state_vector_generated.h. "
        "Compila primero el proyecto raíz o pasa -DMOLAB_GENERATED_DIR=<ruta>/generated")
endif()

# Biblioteca del plugin.
add_library(structures SHARED
    structures.cpp
    structures_module.cpp
)

# Directorios de include.
target_include_directories(structures PRIVATE
    "${MOLAB_ROOT_DIR}/src/api"
    "${MOLAB_ROOT_DIR}/plugins/structures"
    "${GENERATED_INCLUDE_DIR}"
)

# Dependencias de enlace.
target_link_libraries(structures PRIVATE
    flatbuffers::flatbuffers
    nlohmann_json::nlohmann_json
)

# Configuración del compilador.
target_compile_features(structures PRIVATE cxx_std_17)
target_compile_options(structures PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fPIC>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
)

# Directorios de salida (layout simple para build local del plugin).
set_target_properties(structures PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# Nomenclatura específica por plataforma.
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

# Instalación del plugin.
install(TARGETS structures
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)
