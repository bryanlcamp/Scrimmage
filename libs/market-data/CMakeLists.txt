cmake_minimum_required(VERSION 3.20)
project(market-data LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(market-data INTERFACE)

# Include headers directly
target_include_directories(market-data INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}  # MarketDataPipeline.h, OrderBookFeed.h, etc.
)