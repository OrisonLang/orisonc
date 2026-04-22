add_library(orison_driver STATIC
    src/compiler_app.cpp
)

target_include_directories(orison_driver
    PUBLIC
        include
)

target_link_libraries(orison_driver
    PUBLIC
        orison_diagnostics
        orison_source
        orison_syntax
)

target_compile_features(orison_driver PUBLIC cxx_std_23)
