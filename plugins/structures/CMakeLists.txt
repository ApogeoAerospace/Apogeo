# CMakeLists.txt for Structures Plugin

# Plugin library
add_library(structures SHARED
    structures.cpp
)

# Include directories
target_include_directories(structures PRIVATE
    ${CMAKE_SOURCE_DIR}/src/api
    ${CMAKE_BINARY_DIR}/generated
)

# Link FlatBuffers
target_link_libraries(structures PRIVATE
    flatbuffers::flatbuffers
    nlohmann_json::nlohmann_json
)

# Compiler settings
target_compile_features(structures PRIVATE cxx_std_17)
target_compile_options(structures PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-fPIC>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wextra>
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O2>
)

# Set output directory
set_target_properties(structures PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
)

# Platform-specific settings
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

# Install plugin
install(TARGETS structures
    LIBRARY DESTINATION plugins
    RUNTIME DESTINATION plugins
)
