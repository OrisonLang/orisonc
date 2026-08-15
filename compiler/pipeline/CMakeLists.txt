add_library(orison_pipeline STATIC
    src/computed_cleanup_proof_model.cpp
    src/computed_cleanup_proof_reports.cpp
    src/compile_pipeline.cpp
    src/dynamic_array_cleanup_metadata.cpp
    src/dynamic_array_cleanup_metadata_stage.cpp
    src/dynamic_array_cleanup_readiness.cpp
    src/drop_readiness_source_correlation_report.cpp
    src/link_library_collection.cpp
    src/llvm_emission_stage.cpp
    src/lowering_emission_reports.cpp
    src/lowering_emission_options.cpp
    src/runtime_indexed_cleanup_ir_composition.cpp
    src/runtime_indexed_member_cleanup_execution_summary.cpp
    src/semantic_analysis_stage.cpp
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
