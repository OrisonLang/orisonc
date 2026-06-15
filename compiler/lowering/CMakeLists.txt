add_library(orison_lowering STATIC
    src/call_emitter.cpp
    src/c_abi_adapter.cpp
    src/conditional_emitter.cpp
    src/conditional_plan.cpp
    src/control_flow_emitter.cpp
    src/expression_emitter.cpp
    src/function_emitter.cpp
    src/function_signature.cpp
    src/branch_binding_scope.cpp
    src/llvm_cfg.cpp
    src/llvm_ir_emitter.cpp
    src/llvm_object_emitter.cpp
    src/llvm_ir_verifier.cpp
    src/llvm_names.cpp
    src/lowering_context.cpp
    src/lowering_diagnostics.cpp
    src/merge_emitter.cpp
    src/merge_plan.cpp
    src/module_prelude.cpp
    src/statement_emitter.cpp
    src/string_constants.cpp
    src/switch_emitter.cpp
    src/switch_plan.cpp
    src/type_lowering.cpp
    src/while_emitter.cpp
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
