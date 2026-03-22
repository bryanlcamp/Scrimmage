cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)

project(scrimmage-app))

add_executable(scrimmage-app main.cpp)
target_include_directories(scrimmage-app PRIVATE "../src/match-engine/")
target_link_libraries(scrimmage-app INTERFACE matching-engine)