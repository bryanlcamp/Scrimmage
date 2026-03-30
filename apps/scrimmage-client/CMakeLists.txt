cmake_minimum_required(VERSION 3.20)

add_executable(scrimmage-client
    main.cpp
)

target_link_libraries(scrimmage-client
    PRIVATE
        common
        concurrency
        networking
        market-data
        match
)