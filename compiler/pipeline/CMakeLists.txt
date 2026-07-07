add_library(orison_pipeline STATIC
    src/compile_pipeline.cpp
    src/drop_readiness_source_correlation_report.cpp
)

target_include_directories(orison_pipeline
    PUBLIC
        include
)

target_link_libraries(orison_pipeline
    PUBLIC
        orison_lowering
        orison_semantics
        orison_source
        orison_syntax
)

target_compile_features(orison_pipeline PUBLIC cxx_std_23)
