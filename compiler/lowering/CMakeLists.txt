add_library(orison_lowering STATIC
    src/llvm_ir_emitter.cpp
    src/llvm_ir_verifier.cpp
)

target_include_directories(orison_lowering
    PUBLIC
        include
    SYSTEM PRIVATE
        ${LLVM_INCLUDE_DIRS}
)

target_link_libraries(orison_lowering
    PUBLIC
        orison_diagnostics
        orison_semantics
        orison_syntax
    PRIVATE
        LLVM
)

target_compile_features(orison_lowering PUBLIC cxx_std_23)
