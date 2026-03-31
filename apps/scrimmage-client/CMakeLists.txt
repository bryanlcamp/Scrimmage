# apps/scrimmage-client/CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(scrimmage-client LANGUAGES CXX)

# Ensure C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Declare the executable
add_executable(scrimmage-client main.cpp)

# Link header-only libraries
target_link_libraries(scrimmage-client PRIVATE
    common
    concurrency
    market-data
    match
    networking
)

# Optional: add include directories if you have additional headers in apps/
target_include_directories(scrimmage-client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})