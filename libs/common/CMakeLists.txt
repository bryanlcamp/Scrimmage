cmake_minimum_required(VERSION 3.20)
project(common LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library
add_library(common INTERFACE)

# Include base folder and subfolders
target_include_directories(common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}                  # common/
    ${CMAKE_CURRENT_SOURCE_DIR}/SpScRingBuffer  # common/SpScRingBuffer/
)