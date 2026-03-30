cmake_minimum_required(VERSION 3.20)

add_library(market_data STATIC
    MarketDataPipeline.cpp
    OrderBookFeed.cpp
    SyntheticDataInjector.cpp
)

target_include_directories(market_data
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(market_data
    PUBLIC
        networking
        common
)

target_compile_features(market_data PUBLIC cxx_std_20)