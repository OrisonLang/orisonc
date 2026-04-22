add_executable(orisonc
    main.cpp
)

target_link_libraries(orisonc
    PRIVATE
        orison_driver
)

target_compile_features(orisonc PRIVATE cxx_std_23)
