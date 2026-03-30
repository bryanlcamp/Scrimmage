cmake_minimum_required(VERSION 3.20)

add_executable(scrimmage-server
    main.cpp
)

target_link_libraries(scrimmage-server
    PRIVATE
        common
        concurrency
        networking
        market-data
        match
)