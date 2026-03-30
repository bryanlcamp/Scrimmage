# Minimum required CMake version
cmake_minimum_required(VERSION 3.20)

# Create an INTERFACE library since it's header-only
add_library(networking INTERFACE)

# Include directories for this library
target_include_directories(concurrency
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}  # PinnedThread.h is here
)

# Optional: enforce C++17 (or later)
target_compile_features(concurrency INTERFACE cxx_std_20)