# CMakeLists.txt for Test Force Plugin

# Plugin library
add_library(test_force SHARED
    test_force_plugin.cpp
)

# Include directories
target_include_directories(test_force PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/generated
)

# Compiler settings
target_compile_features(test_force PRIVATE cxx_std_17)
target_compile_options(test_force PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fPIC>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
)

# Set output directory
set_target_properties(test_force PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

# Platform-specific settings
if(WIN32)
    set_target_properties(test_force PROPERTIES
        PREFIX ""
        SUFFIX ".dll"
    )
elseif(APPLE)
    set_target_properties(test_force PROPERTIES
        PREFIX "lib"
        SUFFIX ".dylib"
    )
else()
    set_target_properties(test_force PROPERTIES
        PREFIX "lib"
        SUFFIX ".so"
    )
endif()

# Install plugin
install(TARGETS test_force
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)
