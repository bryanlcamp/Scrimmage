# apps/scrimmage-server/CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(scrimmage-server LANGUAGES CXX)

# Ensure C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Declare the executable
add_executable(scrimmage-server main.cpp)

# Link all header-only libraries
target_link_libraries(scrimmage-server PRIVATE
    common
    concurrency
    market-data
    match
    networking
)

# Optional: include the app folder itself for local headers
target_include_directories(scrimmage-server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})