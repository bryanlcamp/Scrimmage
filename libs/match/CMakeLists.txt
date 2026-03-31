cmake_minimum_required(VERSION 3.20)
project(match LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(match INTERFACE)

# Include headers directly
target_include_directories(match INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}  # MatchingEngine.h, OrderBook.h, Order.h, etc.
)