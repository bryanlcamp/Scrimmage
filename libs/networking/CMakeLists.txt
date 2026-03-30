cmake_minimum_required(VERSION 3.20)

add_library(networking INTERFACE)

target_include_directories(networking
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(networking INTERFACE cxx_std_20)