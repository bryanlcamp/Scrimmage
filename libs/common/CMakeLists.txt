# Minimum CMake version required
cmake_minimum_required(VERSION 3.20)

# Create a library target called "common"
# Since you only have headers, you can make it INTERFACE
# If you have .cpp files, change to add_library(common STATIC ...)
add_library(common INTERFACE)

# Include the current directory and any subdirectories where headers are
target_include_directories(common
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}               # For CpuPause.h
    ${CMAKE_CURRENT_SOURCE_DIR}/SpScRingBuffer  # For SpScRingBuffer headers
)

# Optionally, you can specify C++ standard (in case users forget)
target_compile_features(common INTERFACE cxx_std_20)