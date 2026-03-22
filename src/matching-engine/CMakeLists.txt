cmake_minimum_required(VERSION 3.20)
project(matching-engine-lib LANGUAGES CXX)

add_library(matching-enging-lib INTERFACE
            Common/CommonEnums.h
            Common/MatchResult.h
            Defaults.h
            OrderBook.h
            OrderBookEntry.h
            OrderBookNode.h
            OrderPool.h
            )

  target_include_directories(matching-enging-lib INTERFACE "${CMAKE_SOURCE_DIR}/libs") # If headers are in a spe