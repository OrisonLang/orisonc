add_library(orison_diagnostics STATIC
    src/diagnostic_bag.cpp
)

target_include_directories(orison_diagnostics
    PUBLIC
        include
)

target_compile_features(orison_diagnostics PUBLIC cxx_std_23)
