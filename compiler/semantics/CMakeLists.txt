add_library(orison_semantics STATIC
    src/module_semantic_analyzer.cpp
)

target_include_directories(orison_semantics
    PUBLIC
        include
)

target_link_libraries(orison_semantics
    PUBLIC
        orison_diagnostics
        orison_syntax
)

target_compile_features(orison_semantics PUBLIC cxx_std_23)
