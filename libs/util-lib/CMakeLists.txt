add_library(util-lib
    src/util.cpp
)

target_include_directories(util-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
