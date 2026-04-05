# CMakeLists.txt for Environment Plugin

# Plugin library
add_library(environment SHARED
    environment_plugin.cpp
)

# Include directories
target_include_directories(environment PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

# Link FlatBuffers
target_link_libraries(environment PRIVATE
    flatbuffers::flatbuffers
)

# Compiler settings
target_compile_features(environment PRIVATE cxx_std_17)
target_compile_options(environment PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fPIC>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O2>
)

# Set output directory
set_target_properties(environment PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

# Platform-specific settings
if(WIN32)
    set_target_properties(environment PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(environment PROPERTIES
        PREFIX "lib"
        SUFFIX ".dylib"
    )
else()
    set_target_properties(environment PROPERTIES
        PREFIX "lib"
        SUFFIX ".so"
    )
endif()

# Install plugin
install(TARGETS environment
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)
