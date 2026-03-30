cmake_minimum_required(VERSION 3.25)

project(scrimmage-platform
    VERSION 0.1.0
    DESCRIPTION "Low-latency trading / matching engine platform"
    LANGUAGES CXX
)

# ---- Global settings ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands (useful for clangd, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---- Options ----
option(ENABLE_TESTS "Build tests" ON)
option(ENABLE_WARNINGS "Enable strict warnings" ON)

# ---- Warnings (optional but recommended) ----
if(ENABLE_WARNINGS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
        add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    elseif(MSVC)
        add_compile_options(/W4 /WX)
    endif()
endif()

# ---- Dependencies (vendor or find_package can go here) ----
# add_subdirectory(vendor/xyz)

# ---- Libraries ----
add_subdirectory(libs/common)
add_subdirectory(libs/concurrency)
add_subdirectory(libs/market-data)
add_subdirectory(libs/match)
add_subdirectory(libs/networking)

# ---- Applications ----
add_subdirectory(apps/scrimmage-client)
add_subdirectory(apps/scrimmage-server)

# ---- Tests ----
if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()