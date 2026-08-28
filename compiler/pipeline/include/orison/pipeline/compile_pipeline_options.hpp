#pragma once

#include "orison/lowering/lowering_options.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"

#include <vector>

namespace orison::pipeline {

enum class RuntimeIndexedCleanupIrShapeFaultInjection {
    None,
    OmitDescriptorDeallocateTail,
    OmitInlineZeroStore,
    OmitDropCall,
    OmitConditionBlock,
};

enum class DynamicArrayDescriptorLifetimePlanFaultInjection {
    None,
    MismatchCleanupPlanOwners,
};

struct CompilePipelineOptions {
    std::vector<semantics::DropImplementation> test_only_semantic_drop_implementations;
    std::vector<semantics::DropImplementationCandidate> test_only_semantic_drop_implementation_candidates;
    std::vector<semantics::DropLoweringAuthorization> test_only_semantic_drop_lowering_authorizations;
    std::vector<lowering::FixtureDynamicArrayConstructionRequest> fixture_dynamic_array_construction_requests;
    bool test_only_enable_source_drop_lowering = false;
    bool source_drop_lowering_enabled = false;
    bool fixture_derive_dynamic_array_cleanup_from_semantics = false;
    bool dynamic_array_descriptor_cleanup_planning_enabled = false;
    bool fixture_enable_dynamic_array_parameter_descriptors = false;
    bool dynamic_array_parameter_descriptor_audit_bindings_enabled = false;
    bool fixture_emit_bound_dynamic_array_parameter_cleanups = false;
    bool test_only_render_dynamic_array_element_drop_walks = false;
    bool collect_computed_dynamic_array_for_descriptor_renders = false;
    bool collect_computed_dynamic_array_for_loop_control_renders = false;
    bool collect_computed_dynamic_array_for_element_address_renders = false;
    bool collect_computed_dynamic_array_for_element_load_renders = false;
    bool collect_computed_dynamic_array_for_loop_continue_renders = false;
    bool collect_computed_dynamic_array_for_loop_render_sequences = false;
    bool collect_computed_dynamic_array_for_loop_exit_cleanups = false;
    bool collect_computed_dynamic_array_for_cleanup_transitions = false;
    bool collect_computed_dynamic_array_for_production_emission_gates = false;
    bool collect_computed_dynamic_array_for_production_sequences = false;
    bool emit_computed_dynamic_array_for_production_sequence_comments = false;
    bool fixture_authorize_computed_dynamic_array_cleanup_calls = false;
    bool fixture_insert_computed_dynamic_array_cleanup_calls = false;
    bool collect_aggregate_projection_access_metadata = false;
    bool collect_runtime_indexed_cleanup_audit = false;
    bool runtime_indexed_cleanup_emission_enabled = false;
    bool runtime_indexed_cleanup_module_ir_insertion_enabled = false;
    bool runtime_indexed_cleanup_module_ir_mutation_enabled = false;
    bool runtime_indexed_cleanup_function_ir_module_rewrite_enabled = false;
    bool runtime_indexed_cleanup_verified_function_ir_rewrite_enabled = false;
    bool runtime_indexed_cleanup_source_drop_emission_enabled = false;
    bool runtime_indexed_constructor_move_enabled = false;
    bool runtime_indexed_member_cleanup_ir_mutation_enabled = false;
    bool runtime_indexed_member_cleanup_production_gate_enabled = false;
    bool runtime_indexed_member_cleanup_apply_authorization_enabled = false;
    bool runtime_indexed_member_cleanup_rewrite_execution_enabled = false;
    RuntimeIndexedCleanupIrShapeFaultInjection test_only_runtime_indexed_cleanup_ir_shape_fault =
        RuntimeIndexedCleanupIrShapeFaultInjection::None;
    DynamicArrayDescriptorLifetimePlanFaultInjection test_only_dynamic_array_descriptor_lifetime_plan_fault =
        DynamicArrayDescriptorLifetimePlanFaultInjection::None;
    bool runtime_indexed_fixed_array_constructor_move_only = false;
    bool suppress_computed_dynamic_array_cleanup_handoff_metadata = false;
    bool suppress_computed_dynamic_array_cleanup_operand_metadata = false;
    bool dynamic_array_local_lowering_enabled = true;
    bool dynamic_array_parameter_lowering_enabled = true;
    bool dynamic_array_production_signature_lowering_enabled = false;
    bool dynamic_array_production_construction_lowering_enabled = false;
    bool dynamic_array_production_index_lowering_enabled = false;
    bool dynamic_array_production_length_lowering_enabled = false;
    bool dynamic_array_production_for_lowering_enabled = false;
    bool dynamic_array_production_append_lowering_enabled = false;
    bool dynamic_array_production_cleanup_emission_enabled = false;
    bool computed_dynamic_array_local_cleanup_call_insertion_enabled = true;
};

inline auto production_compile_pipeline_options() -> CompilePipelineOptions {
    auto options = CompilePipelineOptions {};
    options.runtime_indexed_cleanup_emission_enabled = true;
    options.runtime_indexed_cleanup_verified_function_ir_rewrite_enabled = true;
    options.runtime_indexed_constructor_move_enabled = true;
    options.runtime_indexed_member_cleanup_ir_mutation_enabled = true;
    options.runtime_indexed_member_cleanup_production_gate_enabled = true;
    options.runtime_indexed_member_cleanup_apply_authorization_enabled = true;
    options.runtime_indexed_member_cleanup_rewrite_execution_enabled = true;
    return options;
}

}  // namespace orison::pipeline
