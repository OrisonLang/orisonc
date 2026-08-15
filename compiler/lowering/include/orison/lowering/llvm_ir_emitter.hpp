#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/computed_dynamic_array_cleanup_call.hpp"
#include "orison/lowering/computed_dynamic_array_cleanup_handoff.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/consumed_descriptor_finalization.hpp"
#include "orison/lowering/dynamic_array_cleanup_capability.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/lowering_options.hpp"
#include "orison/lowering/module_symbol_registry.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct ComputedDynamicArrayForProductionSequenceMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForDescriptorRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string descriptor_storage_name;
    std::string descriptor_value_name;
    std::string data_pointer_name;
    std::string length_name;
    std::string capacity_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForLoopControlRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string condition_block_name;
    std::string body_block_name;
    std::string continue_block_name;
    std::string exit_block_name;
    std::string index_name;
    std::string next_index_name;
    std::string bounds_check_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForElementAddressRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string data_pointer_name;
    std::string index_name;
    std::string element_address_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForElementLoadRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string element_llvm_type_name;
    std::string element_address_name;
    std::string item_value_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForLoopContinueRenderMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string continue_block_name;
    std::string condition_block_name;
    std::string index_name;
    std::string next_index_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForLoopRenderSequenceMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string body_block_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForLoopExitCleanupMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string exit_block_name;
    std::string loop_entry_cleanup_owner_name;
    std::string loop_exit_cleanup_owner_name;
    std::string cleanup_resumption_operation_name;
    std::vector<std::string> rendered_ir;
};

struct ComputedDynamicArrayForCleanupTransitionMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::string acquisition_source_owner_name;
    std::string acquisition_target_owner_name;
    std::string acquisition_operation_name;
    std::string resumption_source_owner_name;
    std::string resumption_target_owner_name;
    std::string resumption_operation_name;
};

struct ComputedDynamicArrayForConsumedCleanupDescriptorMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string source_type_name;
    std::string element_source_type_name;
    ConsumedDescriptorFinalizationPlan finalization_plan;
};

struct ComputedDynamicArrayForProductionEmissionGateMetadata {
    std::string enclosing_function_name;
    std::size_t source_line = 0;
    std::string cleanup_owner_name;
    std::string source_type_name;
    std::string element_source_type_name;
    std::vector<std::string> rendered_ir;
    bool ownership_ready = false;
    bool loop_render_ready = false;
    bool loop_cleanup_ownership_ready = false;
    bool function_cleanup_resumption_ready = false;
    bool exit_cleanup_ready = false;
    bool production_sequence_render_planned = false;
    bool production_emission_enabled = false;
};

auto format_computed_dynamic_array_for_production_sequence_metadata(
    ComputedDynamicArrayForProductionSequenceMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_production_sequence_metadata_report(
    std::vector<ComputedDynamicArrayForProductionSequenceMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_descriptor_render_metadata(
    ComputedDynamicArrayForDescriptorRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_descriptor_render_metadata_report(
    std::vector<ComputedDynamicArrayForDescriptorRenderMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_loop_control_render_metadata(
    ComputedDynamicArrayForLoopControlRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_loop_control_render_metadata_report(
    std::vector<ComputedDynamicArrayForLoopControlRenderMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_element_address_render_metadata(
    ComputedDynamicArrayForElementAddressRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_element_address_render_metadata_report(
    std::vector<ComputedDynamicArrayForElementAddressRenderMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_element_load_render_metadata(
    ComputedDynamicArrayForElementLoadRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_element_load_render_metadata_report(
    std::vector<ComputedDynamicArrayForElementLoadRenderMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_loop_continue_render_metadata(
    ComputedDynamicArrayForLoopContinueRenderMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_loop_continue_render_metadata_report(
    std::vector<ComputedDynamicArrayForLoopContinueRenderMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_loop_render_sequence_metadata(
    ComputedDynamicArrayForLoopRenderSequenceMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_loop_render_sequence_metadata_report(
    std::vector<ComputedDynamicArrayForLoopRenderSequenceMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_loop_exit_cleanup_metadata(
    ComputedDynamicArrayForLoopExitCleanupMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_loop_exit_cleanup_metadata_report(
    std::vector<ComputedDynamicArrayForLoopExitCleanupMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_cleanup_transition_metadata(
    ComputedDynamicArrayForCleanupTransitionMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_cleanup_transition_metadata_report(
    std::vector<ComputedDynamicArrayForCleanupTransitionMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_consumed_cleanup_descriptor_metadata(
    ComputedDynamicArrayForConsumedCleanupDescriptorMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_consumed_cleanup_descriptor_metadata_report(
    std::vector<ComputedDynamicArrayForConsumedCleanupDescriptorMetadata> const& metadata
) -> std::vector<std::string>;

auto format_computed_dynamic_array_for_production_emission_gate_metadata(
    ComputedDynamicArrayForProductionEmissionGateMetadata const& metadata
) -> std::string;

auto format_computed_dynamic_array_for_production_emission_gate_metadata_report(
    std::vector<ComputedDynamicArrayForProductionEmissionGateMetadata> const& metadata
) -> std::vector<std::string>;

struct LlvmIrEmissionResult {
    diagnostics::DiagnosticBag diagnostics;
    std::string ir_text;
    std::vector<ConcurrencyDropCleanupPlan> drop_cleanups;
    std::vector<PlannedDropAction> planned_drop_actions;
    std::vector<PlannedDropDeclaration> planned_drop_declarations;
    std::vector<DynamicArrayRuntimeOperation> dynamic_array_runtime_operations;
    std::vector<DynamicArrayConstructionPlan> dynamic_array_construction_plans;
    std::vector<DynamicArrayDescriptorCleanupPlan> dynamic_array_descriptor_cleanup_plans;
    std::vector<DynamicArrayCleanupObligation> dynamic_array_cleanup_obligations;
    std::vector<DynamicArrayCleanupSequencePlan> dynamic_array_cleanup_sequence_plans;
    std::vector<DynamicArrayCleanupSequenceVerification> dynamic_array_cleanup_sequence_verifications;
    std::optional<DynamicArrayCleanupEmissionCapability> dynamic_array_cleanup_emission_capability;
    std::vector<DynamicArrayCleanupObligationRecord> emitted_dynamic_array_cleanup_obligations;
    std::vector<DynamicArrayCleanupSequencePlanRecord> emitted_dynamic_array_cleanup_sequence_plans;
    std::vector<DynamicArrayCleanupSequenceVerificationRecord> emitted_dynamic_array_cleanup_sequence_verifications;
    std::vector<DynamicArrayCleanupEmissionCapabilityRecord> emitted_dynamic_array_cleanup_emission_capabilities;
    std::vector<AggregateProjectionAccessPlanRecord> aggregate_projection_access_plans;
    std::vector<ComputedDynamicArrayCleanupStateHandoff> computed_dynamic_array_inserted_cleanup_handoffs;
    std::vector<ComputedDynamicArrayCleanupCallOperands> computed_dynamic_array_cleanup_call_operands;
    std::vector<std::string> dynamic_array_allocation_call_ir;
    std::vector<std::string> test_only_dynamic_array_allocation_call_ir;
    std::vector<std::string> test_only_dynamic_array_grow_call_ir;
    std::vector<std::string> test_only_dynamic_array_deallocation_call_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_binding_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_projection_ir;
    std::vector<std::string> test_only_dynamic_array_bounds_check_ir;
    std::vector<std::string> test_only_dynamic_array_element_address_ir;
    std::vector<std::string> test_only_dynamic_array_element_load_ir;
    std::vector<std::string> test_only_dynamic_array_element_store_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_length_update_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_write_back_ir;
    std::vector<std::string> test_only_dynamic_array_append_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_grow_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_append_with_grow_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_cleanup_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_descriptor_load_cleanup_sequence_ir;
    std::vector<std::string> test_only_dynamic_array_element_drop_walk_ir;
    std::vector<ComputedDynamicArrayForProductionSequenceMetadata>
        computed_dynamic_array_for_production_sequences;
    std::vector<std::string> computed_dynamic_array_for_production_sequence_ir;
    std::vector<std::string> computed_dynamic_array_for_production_sequence_module_ir;
    std::vector<ComputedDynamicArrayForDescriptorRenderMetadata>
        computed_dynamic_array_for_descriptor_renders;
    std::vector<std::string> computed_dynamic_array_for_descriptor_render_ir;
    std::vector<ComputedDynamicArrayForLoopControlRenderMetadata>
        computed_dynamic_array_for_loop_control_renders;
    std::vector<std::string> computed_dynamic_array_for_loop_control_render_ir;
    std::vector<ComputedDynamicArrayForElementAddressRenderMetadata>
        computed_dynamic_array_for_element_address_renders;
    std::vector<std::string> computed_dynamic_array_for_element_address_render_ir;
    std::vector<ComputedDynamicArrayForElementLoadRenderMetadata>
        computed_dynamic_array_for_element_load_renders;
    std::vector<std::string> computed_dynamic_array_for_element_load_render_ir;
    std::vector<ComputedDynamicArrayForLoopContinueRenderMetadata>
        computed_dynamic_array_for_loop_continue_renders;
    std::vector<std::string> computed_dynamic_array_for_loop_continue_render_ir;
    std::vector<ComputedDynamicArrayForLoopRenderSequenceMetadata>
        computed_dynamic_array_for_loop_render_sequences;
    std::vector<std::string> computed_dynamic_array_for_loop_render_sequence_ir;
    std::vector<ComputedDynamicArrayForLoopExitCleanupMetadata>
        computed_dynamic_array_for_loop_exit_cleanups;
    std::vector<std::string> computed_dynamic_array_for_loop_exit_cleanup_ir;
    std::vector<ComputedDynamicArrayForCleanupTransitionMetadata>
        computed_dynamic_array_for_cleanup_transitions;
    std::vector<ConsumedDescriptorFinalizationPlan> consumed_descriptor_finalization_plans;
    std::vector<ComputedDynamicArrayForConsumedCleanupDescriptorMetadata>
        computed_dynamic_array_for_consumed_cleanup_descriptors;
    std::vector<ComputedDynamicArrayForProductionEmissionGateMetadata>
        computed_dynamic_array_for_production_emission_gates;
    std::vector<std::string> computed_dynamic_array_for_production_emission_gate_ir;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;
    std::vector<RuntimeIndexedCleanupCapability> runtime_indexed_cleanup_capabilities;
    std::vector<RuntimeIndexedCleanupEmissionPlan> runtime_indexed_cleanup_emission_plans;
    std::vector<RuntimeIndexedMemberCleanupSiblingField> runtime_indexed_member_cleanup_sibling_fields;
    std::vector<RuntimeIndexedMemberCleanupHelperDropBindings>
        runtime_indexed_member_cleanup_helper_drop_bindings;
    std::vector<RuntimeIndexedMemberCleanupFunctionRewriteEditScriptPlan>
        runtime_indexed_member_cleanup_function_rewrite_edit_script_plans;
    std::vector<RuntimeIndexedMemberCleanupProductionReadiness>
        runtime_indexed_member_cleanup_production_readiness;
    std::vector<RuntimeIndexedMemberCleanupMutationProductionReadiness>
        runtime_indexed_member_cleanup_mutation_production_readiness;
    std::vector<RuntimeIndexedMemberCleanupMutationReadinessVerdict>
        runtime_indexed_member_cleanup_mutation_readiness_verdicts;
    std::vector<RuntimeIndexedMemberCleanupMutationRewriteAuthorization>
        runtime_indexed_member_cleanup_mutation_rewrite_authorizations;
    std::vector<RuntimeIndexedMemberCleanupMutationRewriteExecutionPlan>
        runtime_indexed_member_cleanup_mutation_rewrite_execution_plans;
    std::vector<RuntimeIndexedMemberCleanupMutationRewriteExecutionVerdict>
        runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts;
    std::vector<RuntimeIndexedMemberCleanupMutationRewritePromotionStatus>
        runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses;
    std::vector<std::string> runtime_indexed_cleanup_audit_lines;
    std::vector<GeneratedModuleSymbol> generated_module_symbols;
    std::vector<GeneratedModuleSymbol> generated_module_type_symbols;

    auto has_errors() const -> bool;
    auto render(std::string_view path) const -> std::string;
    auto planned_drop_report() const -> std::vector<std::string>;
    auto dynamic_array_construction_plan_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_production_sequence_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_descriptor_render_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_loop_control_render_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_element_address_render_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_element_load_render_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_loop_continue_render_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_loop_render_sequence_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_loop_exit_cleanup_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_cleanup_transition_report() const -> std::vector<std::string>;
    auto consumed_descriptor_finalization_plan_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_consumed_cleanup_descriptor_model_report() const -> std::vector<std::string>;
    auto computed_dynamic_array_for_production_emission_gate_report() const -> std::vector<std::string>;
    auto dynamic_array_runtime_request_report() const -> std::vector<std::string>;
    auto emitted_drop_declaration_report() const -> std::vector<std::string>;
    auto planned_drop_action_report() const -> std::vector<std::string>;
    auto drop_cleanup_authorization_report() const -> std::vector<std::string>;
    auto drop_readiness_snapshot() const -> DropReadinessSnapshot;
    auto drop_readiness_snapshot_report() const -> std::vector<std::string>;
    auto drop_readiness_summary() const -> DropReadinessSummary;
    auto drop_readiness_summary_report() const -> std::vector<std::string>;
    auto drop_readiness_relation_report() const -> std::vector<std::string>;
};

class LlvmIrEmitter {
public:
    auto emit(
        syntax::ModuleSyntax const& module,
        semantics::SemanticAnalysisResult const& semantic_result,
        LlvmIrEmissionOptions const& options = {}
    ) const -> LlvmIrEmissionResult;
    auto emit_metadata(
        syntax::ModuleSyntax const& module,
        semantics::SemanticAnalysisResult const& semantic_result,
        LlvmIrEmissionOptions const& options = {}
    ) const -> LlvmIrEmissionResult;
};

}  // namespace orison::lowering
