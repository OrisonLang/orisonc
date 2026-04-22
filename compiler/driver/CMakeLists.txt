add_library(orison_driver STATIC
    src/compiler_app.cpp
)

target_include_directories(orison_driver
    PUBLIC
        include
)

target_compile_features(orison_driver PUBLIC cxx_std_23)
