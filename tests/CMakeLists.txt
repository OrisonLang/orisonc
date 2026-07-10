add_executable(orison_driver_smoke
    smoke/driver_basic_smoke.cpp
)

target_link_libraries(orison_driver_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_smoke PRIVATE cxx_std_23)

target_compile_definitions(orison_driver_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME orison_driver_smoke
    COMMAND orison_driver_smoke
)

add_executable(orison_driver_artifact_smoke
    smoke/driver_artifact_smoke.cpp
)

target_link_libraries(orison_driver_artifact_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_artifact_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_artifact_smoke
    COMMAND orison_driver_artifact_smoke
)

add_executable(orison_minimal_demo_smoke
    smoke/minimal_demo_smoke.cpp
)

target_link_libraries(orison_minimal_demo_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_minimal_demo_smoke PRIVATE cxx_std_23)

target_compile_definitions(orison_minimal_demo_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME orison_minimal_demo_smoke
    COMMAND orison_minimal_demo_smoke
)

add_executable(orison_driver_ffi_cli_smoke
    smoke/driver_ffi_cli_smoke.cpp
)

target_compile_definitions(orison_driver_ffi_cli_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_driver_ffi_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_ffi_cli_smoke
    COMMAND orison_driver_ffi_cli_smoke
)

add_executable(orison_driver_aggregate_cli_run_smoke
    smoke/driver_aggregate_cli_run_smoke.cpp
)

target_compile_definitions(orison_driver_aggregate_cli_run_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_driver_aggregate_cli_run_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_aggregate_cli_run_smoke
    COMMAND orison_driver_aggregate_cli_run_smoke
)

add_executable(orison_driver_aggregate_access_cli_run_smoke
    smoke/driver_aggregate_access_cli_run_smoke.cpp
)

target_compile_definitions(orison_driver_aggregate_access_cli_run_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_driver_aggregate_access_cli_run_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_aggregate_access_cli_run_smoke
    COMMAND orison_driver_aggregate_access_cli_run_smoke
)

add_executable(orison_driver_array_cli_run_smoke
    smoke/driver_array_cli_run_smoke.cpp
)

target_compile_definitions(orison_driver_array_cli_run_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_driver_array_cli_run_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_array_cli_run_smoke
    COMMAND orison_driver_array_cli_run_smoke
)

add_executable(orison_driver_pointer_record_cli_run_smoke
    smoke/driver_pointer_record_cli_run_smoke.cpp
)

target_compile_definitions(orison_driver_pointer_record_cli_run_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_driver_pointer_record_cli_run_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_pointer_record_cli_run_smoke
    COMMAND orison_driver_pointer_record_cli_run_smoke
)

add_executable(orison_driver_choice_constructor_cli_smoke
    smoke/driver_choice_constructor_cli_smoke.cpp
)

target_compile_features(orison_driver_choice_constructor_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_choice_constructor_cli_smoke
    COMMAND orison_driver_choice_constructor_cli_smoke
)

add_executable(orison_driver_final_expression_cli_smoke
    smoke/driver_final_expression_cli_smoke.cpp
)

target_compile_features(orison_driver_final_expression_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_final_expression_cli_smoke
    COMMAND orison_driver_final_expression_cli_smoke
)

add_executable(orison_driver_choice_final_expression_cli_smoke
    smoke/driver_choice_final_expression_cli_smoke.cpp
)

target_compile_features(orison_driver_choice_final_expression_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_choice_final_expression_cli_smoke
    COMMAND orison_driver_choice_final_expression_cli_smoke
)

add_executable(orison_driver_choice_ternary_cli_smoke
    smoke/driver_choice_ternary_cli_smoke.cpp
)

target_compile_features(orison_driver_choice_ternary_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_choice_ternary_cli_smoke
    COMMAND orison_driver_choice_ternary_cli_smoke
)

add_executable(orison_driver_choice_inference_cli_smoke
    smoke/driver_choice_inference_cli_smoke.cpp
)

target_compile_features(orison_driver_choice_inference_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_choice_inference_cli_smoke
    COMMAND orison_driver_choice_inference_cli_smoke
)

add_executable(orison_driver_array_constant_cli_smoke
    smoke/driver_array_constant_cli_smoke.cpp
)

target_compile_features(orison_driver_array_constant_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_array_constant_cli_smoke
    COMMAND orison_driver_array_constant_cli_smoke
)

add_executable(orison_driver_record_constructor_cli_smoke
    smoke/driver_record_constructor_cli_smoke.cpp
)

target_compile_features(orison_driver_record_constructor_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_record_constructor_cli_smoke
    COMMAND orison_driver_record_constructor_cli_smoke
)

add_executable(orison_driver_pointer_cli_smoke
    smoke/driver_pointer_cli_smoke.cpp
)

target_compile_features(orison_driver_pointer_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_pointer_cli_smoke
    COMMAND orison_driver_pointer_cli_smoke
)

add_executable(orison_driver_address_cli_smoke
    smoke/driver_address_cli_smoke.cpp
)

target_compile_features(orison_driver_address_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_address_cli_smoke
    COMMAND orison_driver_address_cli_smoke
)

add_executable(orison_driver_raw_value_cli_smoke
    smoke/driver_raw_value_cli_smoke.cpp
)

target_compile_features(orison_driver_raw_value_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_raw_value_cli_smoke
    COMMAND orison_driver_raw_value_cli_smoke
)

add_executable(orison_driver_generic_cli_smoke
    smoke/driver_generic_cli_smoke.cpp
)

target_compile_features(orison_driver_generic_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_generic_cli_smoke
    COMMAND orison_driver_generic_cli_smoke
)

add_executable(orison_driver_aggregate_field_cli_smoke
    smoke/driver_aggregate_field_cli_smoke.cpp
)

target_compile_definitions(orison_driver_aggregate_field_cli_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_driver_aggregate_field_cli_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_aggregate_field_cli_smoke
    COMMAND orison_driver_aggregate_field_cli_smoke
)

add_executable(orison_driver_drop_report_cli_smoke
    smoke/driver_drop_report_cli_smoke.cpp
)

target_link_libraries(orison_driver_drop_report_cli_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_drop_report_cli_smoke PRIVATE cxx_std_23)

target_compile_definitions(orison_driver_drop_report_cli_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

add_test(
    NAME orison_driver_drop_report_cli_smoke
    COMMAND orison_driver_drop_report_cli_smoke
)

add_executable(orison_driver_aggregate_lowering_smoke
    smoke/driver_aggregate_lowering_smoke.cpp
)

target_link_libraries(orison_driver_aggregate_lowering_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_aggregate_lowering_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_aggregate_lowering_smoke
    COMMAND orison_driver_aggregate_lowering_smoke
)

add_executable(orison_driver_concurrency_lowering_smoke
    smoke/driver_concurrency_lowering_smoke.cpp
)

target_link_libraries(orison_driver_concurrency_lowering_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_concurrency_lowering_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_concurrency_lowering_smoke
    COMMAND orison_driver_concurrency_lowering_smoke
)

add_executable(orison_driver_concurrency_diagnostics_smoke
    smoke/driver_concurrency_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_concurrency_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_concurrency_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_concurrency_diagnostics_smoke
    COMMAND orison_driver_concurrency_diagnostics_smoke
)

add_executable(orison_driver_concurrency_capture_diagnostics_smoke
    smoke/driver_concurrency_capture_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_concurrency_capture_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_concurrency_capture_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_concurrency_capture_diagnostics_smoke
    COMMAND orison_driver_concurrency_capture_diagnostics_smoke
)

add_executable(orison_driver_concurrency_transfer_diagnostics_smoke
    smoke/driver_concurrency_transfer_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_concurrency_transfer_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_concurrency_transfer_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_concurrency_transfer_diagnostics_smoke
    COMMAND orison_driver_concurrency_transfer_diagnostics_smoke
)

add_executable(orison_driver_unsafe_diagnostics_smoke
    smoke/driver_unsafe_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_unsafe_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_unsafe_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_unsafe_diagnostics_smoke
    COMMAND orison_driver_unsafe_diagnostics_smoke
)

add_executable(orison_driver_pointer_address_diagnostics_smoke
    smoke/driver_pointer_address_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_pointer_address_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_pointer_address_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_pointer_address_diagnostics_smoke
    COMMAND orison_driver_pointer_address_diagnostics_smoke
)

add_executable(orison_driver_raw_value_diagnostics_smoke
    smoke/driver_raw_value_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_raw_value_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_raw_value_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_raw_value_diagnostics_smoke
    COMMAND orison_driver_raw_value_diagnostics_smoke
)

add_executable(orison_driver_raw_helper_diagnostics_smoke
    smoke/driver_raw_helper_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_raw_helper_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_raw_helper_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_raw_helper_diagnostics_smoke
    COMMAND orison_driver_raw_helper_diagnostics_smoke
)

add_executable(orison_driver_volatile_diagnostics_smoke
    smoke/driver_volatile_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_volatile_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_volatile_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_volatile_diagnostics_smoke
    COMMAND orison_driver_volatile_diagnostics_smoke
)

add_executable(orison_driver_async_origin_diagnostics_smoke
    smoke/driver_async_origin_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_async_origin_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_async_origin_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_async_origin_diagnostics_smoke
    COMMAND orison_driver_async_origin_diagnostics_smoke
)

add_executable(orison_driver_switch_nested_pattern_diagnostics_smoke
    smoke/driver_switch_nested_pattern_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_switch_nested_pattern_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_switch_nested_pattern_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_switch_nested_pattern_diagnostics_smoke
    COMMAND orison_driver_switch_nested_pattern_diagnostics_smoke
)

add_executable(orison_driver_switch_pattern_diagnostics_smoke
    smoke/driver_switch_pattern_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_switch_pattern_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_switch_pattern_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_switch_pattern_diagnostics_smoke
    COMMAND orison_driver_switch_pattern_diagnostics_smoke
)

add_executable(orison_driver_switch_value_pattern_diagnostics_smoke
    smoke/driver_switch_value_pattern_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_switch_value_pattern_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_switch_value_pattern_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_switch_value_pattern_diagnostics_smoke
    COMMAND orison_driver_switch_value_pattern_diagnostics_smoke
)

add_executable(orison_driver_switch_choice_exhaustiveness_diagnostics_smoke
    smoke/driver_switch_choice_exhaustiveness_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_switch_choice_exhaustiveness_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_switch_choice_exhaustiveness_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_switch_choice_exhaustiveness_diagnostics_smoke
    COMMAND orison_driver_switch_choice_exhaustiveness_diagnostics_smoke
)

add_executable(orison_driver_loop_control_diagnostics_smoke
    smoke/driver_loop_control_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_loop_control_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_loop_control_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_loop_control_diagnostics_smoke
    COMMAND orison_driver_loop_control_diagnostics_smoke
)

add_executable(orison_driver_receiver_diagnostics_smoke
    smoke/driver_receiver_diagnostics_smoke.cpp
)

target_link_libraries(orison_driver_receiver_diagnostics_smoke
    PRIVATE
        orison_driver
)

target_compile_features(orison_driver_receiver_diagnostics_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_driver_receiver_diagnostics_smoke
    COMMAND orison_driver_receiver_diagnostics_smoke
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

add_executable(orison_drop_model_smoke
    smoke/drop_model_smoke.cpp
)

target_link_libraries(orison_drop_model_smoke
    PRIVATE
        orison_semantics
)

target_compile_features(orison_drop_model_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_drop_model_smoke
    COMMAND orison_drop_model_smoke
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

add_executable(orison_concurrency_runtime_smoke
    smoke/concurrency_runtime_smoke.cpp
)

target_link_libraries(orison_concurrency_runtime_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_concurrency_runtime_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_concurrency_runtime_smoke
    COMMAND orison_concurrency_runtime_smoke
)

add_executable(orison_concurrency_runtime_abi_smoke
    smoke/concurrency_runtime_abi_smoke.cpp
)

target_link_libraries(orison_concurrency_runtime_abi_smoke
    PRIVATE
        orison_runtime_concurrency
)

target_compile_features(orison_concurrency_runtime_abi_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_concurrency_runtime_abi_smoke
    COMMAND orison_concurrency_runtime_abi_smoke
)

add_executable(orison_concurrency_plan_smoke
    smoke/concurrency_plan_smoke.cpp
)

target_link_libraries(orison_concurrency_plan_smoke
    PRIVATE
        orison_lowering
        orison_semantics
        orison_source
        orison_syntax
)

target_compile_features(orison_concurrency_plan_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_concurrency_plan_smoke
    COMMAND orison_concurrency_plan_smoke
)

add_executable(orison_concurrency_emitter_smoke
    smoke/concurrency_emitter_smoke.cpp
)

target_link_libraries(orison_concurrency_emitter_smoke
    PRIVATE
        orison_lowering
        orison_semantics
        orison_source
        orison_syntax
)

target_compile_features(orison_concurrency_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_concurrency_emitter_smoke
    COMMAND orison_concurrency_emitter_smoke
)

add_executable(orison_drop_cleanup_emitter_smoke
    smoke/drop_cleanup_emitter_smoke.cpp
)

target_link_libraries(orison_drop_cleanup_emitter_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_drop_cleanup_emitter_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_drop_cleanup_emitter_smoke
    COMMAND orison_drop_cleanup_emitter_smoke
)

add_executable(orison_drop_cleanup_abi_smoke
    smoke/drop_cleanup_abi_smoke.cpp
)

target_link_libraries(orison_drop_cleanup_abi_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_drop_cleanup_abi_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_drop_cleanup_abi_smoke
    COMMAND orison_drop_cleanup_abi_smoke
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

add_executable(orison_drop_metadata_smoke
    smoke/drop_metadata_smoke.cpp
)

target_link_libraries(orison_drop_metadata_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_drop_metadata_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_drop_metadata_smoke
    COMMAND orison_drop_metadata_smoke
)

add_executable(orison_syntax_traversal_smoke
    smoke/syntax_traversal_smoke.cpp
)

target_link_libraries(orison_syntax_traversal_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_syntax_traversal_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_syntax_traversal_smoke
    COMMAND orison_syntax_traversal_smoke
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

add_executable(orison_addressable_binding_smoke
    smoke/addressable_binding_smoke.cpp
)

target_link_libraries(orison_addressable_binding_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_addressable_binding_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_addressable_binding_smoke
    COMMAND orison_addressable_binding_smoke
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

add_executable(orison_aggregate_path_smoke
    smoke/aggregate_path_smoke.cpp
)

target_link_libraries(orison_aggregate_path_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_aggregate_path_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_aggregate_path_smoke
    COMMAND orison_aggregate_path_smoke
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

add_executable(orison_source_type_queries_smoke
    smoke/source_type_queries_smoke.cpp
)

target_link_libraries(orison_source_type_queries_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_source_type_queries_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_source_type_queries_smoke
    COMMAND orison_source_type_queries_smoke
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

add_executable(orison_null_safe_plan_smoke
    smoke/null_safe_plan_smoke.cpp
)

target_link_libraries(orison_null_safe_plan_smoke
    PRIVATE
        orison_lowering
        orison_source
        orison_syntax
)

target_compile_features(orison_null_safe_plan_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_null_safe_plan_smoke
    COMMAND orison_null_safe_plan_smoke
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

add_executable(orison_drop_readiness_source_correlation_report_smoke
    smoke/drop_readiness_source_correlation_report_smoke.cpp
)

target_link_libraries(orison_drop_readiness_source_correlation_report_smoke
    PRIVATE
        orison_pipeline
)

target_compile_features(orison_drop_readiness_source_correlation_report_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_drop_readiness_source_correlation_report_smoke
    COMMAND orison_drop_readiness_source_correlation_report_smoke
)

add_executable(orison_drop_cleanup_authorization_report_smoke
    smoke/drop_cleanup_authorization_report_smoke.cpp
)

target_link_libraries(orison_drop_cleanup_authorization_report_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_drop_cleanup_authorization_report_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_drop_cleanup_authorization_report_smoke
    COMMAND orison_drop_cleanup_authorization_report_smoke
)

add_executable(orison_drop_readiness_snapshot_summary_report_smoke
    smoke/drop_readiness_snapshot_summary_report_smoke.cpp
)

target_link_libraries(orison_drop_readiness_snapshot_summary_report_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_drop_readiness_snapshot_summary_report_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_drop_readiness_snapshot_summary_report_smoke
    COMMAND orison_drop_readiness_snapshot_summary_report_smoke
)

add_executable(orison_drop_readiness_blocker_report_smoke
    smoke/drop_readiness_blocker_report_smoke.cpp
)

target_link_libraries(orison_drop_readiness_blocker_report_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_drop_readiness_blocker_report_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_drop_readiness_blocker_report_smoke
    COMMAND orison_drop_readiness_blocker_report_smoke
)

add_executable(orison_drop_readiness_relation_report_smoke
    smoke/drop_readiness_relation_report_smoke.cpp
)

target_link_libraries(orison_drop_readiness_relation_report_smoke
    PRIVATE
        orison_lowering
)

target_compile_features(orison_drop_readiness_relation_report_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_drop_readiness_relation_report_smoke
    COMMAND orison_drop_readiness_relation_report_smoke
)

add_executable(orison_canonical_pipeline_smoke
    smoke/canonical_pipeline_smoke.cpp
)

target_link_libraries(orison_canonical_pipeline_smoke
    PRIVATE
        orison_driver
)

target_compile_definitions(orison_canonical_pipeline_smoke
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_canonical_pipeline_smoke PRIVATE cxx_std_23)

add_test(
    NAME orison_canonical_pipeline_smoke
    COMMAND orison_canonical_pipeline_smoke
)

set_tests_properties(orison_canonical_pipeline_smoke PROPERTIES LABELS "canonical_pipeline")

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

add_executable(orison_smoke_temp_isolation_guard
    smoke/temp_isolation_guard_smoke.cpp
)

target_compile_definitions(orison_smoke_temp_isolation_guard
    PRIVATE
        ORISON_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

target_compile_features(orison_smoke_temp_isolation_guard PRIVATE cxx_std_23)

add_test(
    NAME orison_smoke_temp_isolation_guard
    COMMAND orison_smoke_temp_isolation_guard
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
