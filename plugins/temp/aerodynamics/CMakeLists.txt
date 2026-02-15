# CMakeLists.txt for Aerodynamics Plugin

# Plugin library
add_library(aerodynamics SHARED
    aerodynamics_plugin.cpp
)

# Include directories
target_include_directories(aerodynamics PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

# Link FlatBuffers
target_link_libraries(aerodynamics PRIVATE
    flatbuffers::flatbuffers
)

# Compiler settings
target_compile_features(aerodynamics PRIVATE cxx_std_17)
target_compile_options(aerodynamics PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fPIC>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O2>
)

# Set output directory
set_target_properties(aerodynamics PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

# Platform-specific settings
if(WIN32)
    set_target_properties(aerodynamics PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(aerodynamics PROPERTIES
        PREFIX "lib"
        SUFFIX ".dylib"
    )
else()
    set_target_properties(aerodynamics PROPERTIES
        PREFIX "lib"
        SUFFIX ".so"
    )
endif()

# Install plugin
install(TARGETS aerodynamics
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)
