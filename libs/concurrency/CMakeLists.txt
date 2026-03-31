cmake_minimum_required(VERSION 3.20)
project(concurrency LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(concurrency INTERFACE)

# concurrency headers
target_include_directories(concurrency INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}  # contains CpuPause.h, PinnedThread.h
)