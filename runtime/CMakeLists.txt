add_library(orison_runtime_concurrency STATIC
    src/concurrency_runtime.c
)

target_compile_features(orison_runtime_concurrency PRIVATE c_std_17)
