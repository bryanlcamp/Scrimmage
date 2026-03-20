add_library(math-lib
    src/math.cpp
)

target_include_directories(math-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
