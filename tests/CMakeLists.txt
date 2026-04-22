add_executable(orison_driver_smoke
    smoke/compiler_app_smoke.cpp
)

target_link_libraries(orison_driver_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_smoke
    COMMAND orison_driver_smoke
)
