cmake_minimum_required(VERSION 3.20)

add_library(concurrency INTERFACE)

target_include_directories(concurrency
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(concurrency INTERFACE cxx_std_20)

target_link_libraries(concurrency
    INTERFACE
        networking
)