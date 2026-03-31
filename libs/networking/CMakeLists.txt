cmake_minimum_required(VERSION 3.20)
project(networking LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(networking INTERFACE)

# Include all subfolders
target_include_directories(networking INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}                  # networking/
    ${CMAKE_CURRENT_SOURCE_DIR}/pipeline         # networking/pipeline/
    ${CMAKE_CURRENT_SOURCE_DIR}/tcp              # networking/tcp/
    ${CMAKE_CURRENT_SOURCE_DIR}/udp              # networking/udp/
)