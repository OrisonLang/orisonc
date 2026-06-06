add_library(orison_lowering STATIC
    src/llvm_ir_emitter.cpp
)

target_include_directories(orison_lowering
    PUBLIC
        include
)

target_link_libraries(orison_lowering
    PUBLIC
        orison_diagnostics
        orison_semantics
        orison_syntax
)

target_compile_features(orison_lowering PUBLIC cxx_std_23)
