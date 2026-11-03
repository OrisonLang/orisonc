add_library(orison_driver STATIC
    src/build_plan.cpp
    src/computed_cleanup_reports.cpp
    src/compiler_app.cpp
    src/runtime_indexed_cleanup_reports.cpp
)

target_include_directories(orison_driver
    PUBLIC
        include
    SYSTEM PRIVATE
        ${LLVM_INCLUDE_DIRS}
)

target_link_libraries(orison_driver
    PUBLIC
        orison_diagnostics
        orison_link
        orison_pipeline
    PRIVATE
        LLVM
)

target_compile_features(orison_driver PUBLIC cxx_std_23)
