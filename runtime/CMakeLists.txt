find_package(Threads REQUIRED)

add_library(orison_runtime_concurrency STATIC
    src/concurrency_runtime.cpp
)

target_link_libraries(orison_runtime_concurrency
    PUBLIC
        Threads::Threads
)

target_compile_features(orison_runtime_concurrency PRIVATE cxx_std_23)

if(ORISON_ENABLE_TESTS)
    target_compile_definitions(orison_runtime_concurrency PRIVATE ORISON_RUNTIME_TESTING)
endif()
