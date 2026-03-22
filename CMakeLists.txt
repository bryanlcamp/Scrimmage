cmake_minimum_required(VERSION 3.20)
project(Scrimmage)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

target_link_libraries(Scrimmage, INTERFACE, matching-engine-lib)