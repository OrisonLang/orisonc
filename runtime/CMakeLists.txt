find_package(Threads REQUIRED)

add_library(orison_runtime_concurrency STATIC
    src/concurrency_runtime.c
)

target_link_libraries(orison_runtime_concurrency
    PUBLIC
        Threads::Threads
)

target_compile_features(orison_runtime_concurrency PRIVATE c_std_17)
