# Minimum CMake version
cmake_minimum_required(VERSION 3.20)

# Create a static library for match
add_library(match STATIC
    MatchEngine.cpp
    MatchEngine.h
)

# Include directories for this library
target_include_directories(match
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}   # For MatchEngine.h
)

# Specify dependencies: only common
target_link_libraries(match
    PRIVATE common   # Links common library
)

# Optional: enforce C++17 standard
target_compile_features(match PUBLIC cxx_std_17)