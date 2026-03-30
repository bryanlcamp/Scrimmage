# Minimum CMake version
cmake_minimum_required(VERSION 3.20)

# Create an INTERFACE library for networking (header-only)
add_library(networking INTERFACE)

# Include directories for networking
target_include_directories(networking
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}  # PinnedThread.h location
)

# Networking depends on common
target_link_libraries(networking
    INTERFACE common
)

# Optional: enforce C++17 standard
target_compile_features(networking INTERFACE cxx_std_20)