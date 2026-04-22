add_library(orison_source STATIC
    src/source_file.cpp
)

target_include_directories(orison_source
    PUBLIC
        include
)

target_compile_features(orison_source PUBLIC cxx_std_23)
