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

add_executable(orison_module_parser_smoke
    smoke/module_parser_smoke.cpp
)

target_link_libraries(orison_module_parser_smoke
    PRIVATE
        orison_syntax
)

target_compile_features(orison_module_parser_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_module_parser_smoke
    COMMAND orison_module_parser_smoke
)

add_executable(orisonc_cli_smoke
    smoke/orisonc_cli_smoke.cpp
)

target_compile_features(orisonc_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orisonc_cli_smoke
    COMMAND orisonc_cli_smoke
)
