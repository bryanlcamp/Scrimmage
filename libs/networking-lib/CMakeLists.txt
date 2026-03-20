add_library(networking-lib
    src/network.cpp
)

target_include_directories(networking-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
