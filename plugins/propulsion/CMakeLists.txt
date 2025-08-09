# CMakeLists.txt for Propulsion Plugin

cmake_minimum_required(VERSION 3.16)
project(PropulsionPlugin)

# Plugin library
add_library(propulsion SHARED
    propulsion.cpp
)

# Include directories
target_include_directories(propulsion PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
    /usr/local/include
    ${FLATBUFFERS_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(FLATBUFFERS REQUIRED flatbuffers)
find_package(nlohmann_json REQUIRED)

# Link FlatBuffers and other dependencies
target_link_libraries(propulsion PRIVATE
    ${FLATBUFFERS_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Compiler settings
target_compile_features(propulsion PRIVATE cxx_std_17)
target_compile_options(propulsion PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fPIC>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O2>
)

# Set output directory
set_target_properties(propulsion PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Platform-specific settings
if(WIN32)
    set_target_properties(propulsion PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(propulsion PROPERTIES
        PREFIX "lib"
        SUFFIX ".dylib"
    )
else()
    set_target_properties(propulsion PROPERTIES
        PREFIX "lib"
        SUFFIX ".so"
    )
endif()

# Install plugin
install(TARGETS propulsion
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib
)
