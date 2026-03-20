add_executable(scrimmage-app
    main.cpp
)

target_link_libraries(scrimmage-app PRIVATE
    math-lib
    util-lib
    networking-lib
)
