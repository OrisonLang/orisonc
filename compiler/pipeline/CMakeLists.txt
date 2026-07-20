add_library(orison_pipeline STATIC
    src/compile_pipeline.cpp
    src/dynamic_array_cleanup_readiness.cpp
    src/drop_readiness_source_correlation_report.cpp
    src/lowering_emission_options.cpp
    src/semantic_drop_reports.cpp
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
