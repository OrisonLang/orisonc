add_executable(orison_driver_smoke
    smoke/compiler_app_smoke.cpp
)

target_link_libraries(orison_driver_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_smoke
    COMMAND orison_driver_smoke
)

add_executable(orison_module_parser_smoke
    smoke/module_parser_smoke.cpp
)

target_link_libraries(orison_module_parser_smoke
    PRIVATE
        orison_syntax
)

target_compile_features(orison_module_parser_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_module_parser_smoke
    COMMAND orison_module_parser_smoke
)

add_executable(orison_semantics_smoke
    smoke/semantics_smoke.cpp
)

target_link_libraries(orison_semantics_smoke
    PRIVATE
        orison_semantics
        orison_source
        orison_syntax
)

target_compile_features(orison_semantics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_semantics_smoke
    COMMAND orison_semantics_smoke
)

add_executable(orison_lowering_smoke
    smoke/lowering_smoke.cpp
)

target_link_libraries(orison_lowering_smoke
    PRIVATE
        orison_lowering
        orison_semantics
        orison_source
        orison_syntax
)

target_compile_features(orison_lowering_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_lowering_smoke
    COMMAND orison_lowering_smoke
)

add_executable(orison_c_abi_adapter_smoke
    smoke/c_abi_adapter_smoke.cpp
)

target_link_libraries(orison_c_abi_adapter_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_c_abi_adapter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_c_abi_adapter_smoke
    COMMAND orison_c_abi_adapter_smoke
)

add_executable(orison_call_emitter_smoke
    smoke/call_emitter_smoke.cpp
)

target_link_libraries(orison_call_emitter_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_call_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_call_emitter_smoke
    COMMAND orison_call_emitter_smoke
)

add_executable(orison_type_lowering_smoke
    smoke/type_lowering_smoke.cpp
)

target_link_libraries(orison_type_lowering_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_type_lowering_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_type_lowering_smoke
    COMMAND orison_type_lowering_smoke
)

add_executable(orison_function_signature_smoke
    smoke/function_signature_smoke.cpp
)

target_link_libraries(orison_function_signature_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_function_signature_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_function_signature_smoke
    COMMAND orison_function_signature_smoke
)

add_executable(orison_lowering_context_smoke
    smoke/lowering_context_smoke.cpp
)

target_link_libraries(orison_lowering_context_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_lowering_context_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_lowering_context_smoke
    COMMAND orison_lowering_context_smoke
)

add_executable(orison_string_constants_smoke
    smoke/string_constants_smoke.cpp
)

target_link_libraries(orison_string_constants_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_string_constants_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_string_constants_smoke
    COMMAND orison_string_constants_smoke
)

add_executable(orison_module_prelude_smoke
    smoke/module_prelude_smoke.cpp
)

target_link_libraries(orison_module_prelude_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_module_prelude_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_module_prelude_smoke
    COMMAND orison_module_prelude_smoke
)

add_executable(orison_function_emitter_smoke
    smoke/function_emitter_smoke.cpp
)

target_link_libraries(orison_function_emitter_smoke
    PRIVATE
        orison_lowering
)

target_compile_definitions(orison_function_emitter_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_function_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_function_emitter_smoke
    COMMAND orison_function_emitter_smoke
)

add_executable(orison_expression_emitter_smoke
    smoke/expression_emitter_smoke.cpp
)

target_link_libraries(orison_expression_emitter_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_expression_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_expression_emitter_smoke
    COMMAND orison_expression_emitter_smoke
)

add_executable(orison_statement_emitter_smoke
    smoke/statement_emitter_smoke.cpp
)

target_link_libraries(orison_statement_emitter_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_statement_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_statement_emitter_smoke
    COMMAND orison_statement_emitter_smoke
)

add_executable(orison_member_call_receiver_smoke
    smoke/member_call_receiver_smoke.cpp
)

target_link_libraries(orison_member_call_receiver_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_member_call_receiver_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_member_call_receiver_smoke
    COMMAND orison_member_call_receiver_smoke
)

add_executable(orison_branch_binding_scope_smoke
    smoke/branch_binding_scope_smoke.cpp
)

target_link_libraries(orison_branch_binding_scope_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_branch_binding_scope_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_branch_binding_scope_smoke
    COMMAND orison_branch_binding_scope_smoke
)

add_executable(orison_control_flow_emitter_smoke
    smoke/control_flow_emitter_smoke.cpp
)

target_link_libraries(orison_control_flow_emitter_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_control_flow_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_control_flow_emitter_smoke
    COMMAND orison_control_flow_emitter_smoke
)

add_executable(orison_conditional_plan_smoke
    smoke/conditional_plan_smoke.cpp
)

target_link_libraries(orison_conditional_plan_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_conditional_plan_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_conditional_plan_smoke
    COMMAND orison_conditional_plan_smoke
)

add_executable(orison_conditional_emitter_smoke
    smoke/conditional_emitter_smoke.cpp
)

target_link_libraries(orison_conditional_emitter_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_conditional_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_conditional_emitter_smoke
    COMMAND orison_conditional_emitter_smoke
)

add_executable(orison_switch_plan_smoke
    smoke/switch_plan_smoke.cpp
)

target_link_libraries(orison_switch_plan_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_switch_plan_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_switch_plan_smoke
    COMMAND orison_switch_plan_smoke
)

add_executable(orison_switch_emitter_smoke
    smoke/switch_emitter_smoke.cpp
)

target_link_libraries(orison_switch_emitter_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_switch_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_switch_emitter_smoke
    COMMAND orison_switch_emitter_smoke
)

add_executable(orison_lowering_diagnostics_smoke
    smoke/lowering_diagnostics_smoke.cpp
)

target_link_libraries(orison_lowering_diagnostics_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_lowering_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_lowering_diagnostics_smoke
    COMMAND orison_lowering_diagnostics_smoke
)

add_executable(orison_llvm_names_smoke
    smoke/llvm_names_smoke.cpp
)

target_link_libraries(orison_llvm_names_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_llvm_names_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_llvm_names_smoke
    COMMAND orison_llvm_names_smoke
)

add_executable(orison_llvm_cfg_smoke
    smoke/llvm_cfg_smoke.cpp
)

target_link_libraries(orison_llvm_cfg_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_llvm_cfg_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_llvm_cfg_smoke
    COMMAND orison_llvm_cfg_smoke
)

add_executable(orison_merge_plan_smoke
    smoke/merge_plan_smoke.cpp
)

target_link_libraries(orison_merge_plan_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_merge_plan_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_merge_plan_smoke
    COMMAND orison_merge_plan_smoke
)

add_executable(orison_merge_emitter_smoke
    smoke/merge_emitter_smoke.cpp
)

target_link_libraries(orison_merge_emitter_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_merge_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_merge_emitter_smoke
    COMMAND orison_merge_emitter_smoke
)

add_executable(orison_pipeline_smoke
    smoke/compile_pipeline_smoke.cpp
)

target_link_libraries(orison_pipeline_smoke
    PRIVATE
        orison_pipeline
)

target_compile_definitions(orison_pipeline_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_pipeline_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_pipeline_smoke
    COMMAND orison_pipeline_smoke
)

add_executable(orison_examples_smoke
    smoke/examples_smoke.cpp
)

target_link_libraries(orison_examples_smoke
    PRIVATE
        orison_pipeline
)

target_compile_definitions(orison_examples_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_examples_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_examples_smoke
    COMMAND orison_examples_smoke
)

add_executable(orison_host_linker_smoke
    smoke/host_linker_smoke.cpp
)

target_link_libraries(orison_host_linker_smoke
    PRIVATE
        orison_link
        orison_lowering
)

target_compile_features(orison_host_linker_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_host_linker_smoke
    COMMAND orison_host_linker_smoke
)

add_executable(orison_lexer_smoke
    smoke/lexer_smoke.cpp
)

target_link_libraries(orison_lexer_smoke
    PRIVATE
        orison_syntax
)

target_compile_features(orison_lexer_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_lexer_smoke
    COMMAND orison_lexer_smoke
)

add_executable(orisonc_cli_smoke
    smoke/orisonc_cli_smoke.cpp
)

target_compile_definitions(orisonc_cli_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orisonc_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orisonc_cli_smoke
    COMMAND orisonc_cli_smoke
)
