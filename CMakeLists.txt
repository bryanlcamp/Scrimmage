cmake_minimum_required(VERSION 3.20)
project(Scrimmage)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add libraries
add_subdirectory(libs/math-lib)
add_subdirectory(libs/util-lib)
add_subdirectory(libs/networking-lib)

# Add main application
add_subdirectory(src)
