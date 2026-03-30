# Minimum CMake version
cmake_minimum_required(VERSION 3.20)

# Create a library target for MarketData
add_library(marketdata STATIC
    MarketDataFeed.cpp
    MarketDataFeed.h
)

# Include directories for this library
target_include_directories(marketdata
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}   # For MarketDataFeed.h
)

# Specify dependencies: networking and common
target_link_libraries(marketdata
    PRIVATE networking   # links networking (header-only)
    PRIVATE common       # links common (header-only)
)

# Optional: enforce C++17 standard
target_compile_features(marketdata PUBLIC cxx_std_20)