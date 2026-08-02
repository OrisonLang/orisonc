#pragma once

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/aggregate_projection_access_plan.hpp"
#include "orison/lowering/dynamic_array_cleanup_metadata.hpp"
#include "orison/lowering/lowering_options.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace orison::pipeline {

struct DynamicArrayCleanupAvailability {
    std::vector<std::string> missing_element_drop_pairs;
    bool descriptor_origins_available = false;
    bool descriptor_cleanup_plans_available = false;
    bool cleanup_obligations_available = false;
    bool sequence_verification_available = false;
    bool sequence_verification_passed = false;
    bool cleanup_capability_proven = false;
};

struct DynamicArrayCleanupEmissionCapabilityState {
    std::vector<std::string> function_symbol_names;
    std::vector<std::string> cleanup_pairs;
    std::vector<std::string> cleanup_operation_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> element_drop_pairs;
    std::vector<std::string> missing_element_drop_pairs;
    bool capability_metadata_available = false;
    bool proven = false;
    bool emission_enabled = false;
    bool descriptor_storage_bound = false;
    bool sequence_verified = false;
    bool element_cleanup_authorized_or_not_required = false;
    bool descriptor_deallocation_authorized = false;
};

struct DynamicArrayDescriptorCleanupPlanState {
    std::vector<lowering::DynamicArrayDescriptorCleanupPlan> plans;
};

struct DynamicArrayConstructionPlanState {
    std::vector<lowering::DynamicArrayConstructionPlan> plans;
};

struct DynamicArrayRuntimeRequestState {
    std::vector<lowering::DynamicArrayRuntimeOperation> operations;
};

struct DynamicArrayAllocationCallEmissionState {
    std::vector<std::string> rendered_ir_snippets;
    bool allocation_calls_rendered = false;
    std::size_t rendered_call_count = 0;
};

struct PlannedDropDeclarationState {
    std::vector<lowering::PlannedDropDeclaration> declarations;
};

struct PlannedDropActionState {
    std::vector<lowering::PlannedDropAction> actions;
};

struct DropCleanupAuthorizationState {
    std::vector<lowering::ConcurrencyDropCleanupPlan> cleanups;
    std::vector<lowering::DropCleanupAuthorizationReport> authorizations;
};

struct DynamicArrayCleanupObligationState {
    std::vector<std::string> function_symbol_names;
    std::vector<lowering::DynamicArrayCleanupObligation> obligations;
};

struct DynamicArrayCleanupSequencePlanState {
    std::vector<std::string> function_symbol_names;
    std::vector<lowering::DynamicArrayCleanupSequencePlan> plans;
};

struct DynamicArrayCleanupSequenceVerificationState {
    std::vector<std::string> function_symbol_names;
    std::vector<lowering::DynamicArrayCleanupSequenceVerification> verifications;
};

struct DynamicArrayCleanupProductionReadiness {
    std::vector<std::string> missing_element_drop_pairs;
    bool descriptor_origins_available = false;
    bool descriptor_cleanup_plans_available = false;
    bool cleanup_obligations_available = false;
    bool sequence_verification_available = false;
    bool sequence_verification_passed = false;
    bool cleanup_capability_proven = false;
    bool production_signature_lowering_enabled = false;
    bool production_construction_lowering_enabled = false;
    bool production_cleanup_emission_enabled = false;
};

struct ComputedDynamicArrayForProductionSequenceState {
    std::vector<std::string> cleanup_owner_names;
    bool sequence_metadata_available = false;
    bool module_comments_emitted = false;
    std::size_t sequence_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
    std::size_t module_comment_line_count = 0;
};

struct ComputedDynamicArrayForDescriptorRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> descriptor_storage_names;
    std::vector<std::string> descriptor_value_names;
    std::vector<std::string> data_pointer_names;
    std::vector<std::string> length_names;
    std::vector<std::string> capacity_names;
    bool render_metadata_available = false;
    bool all_descriptor_projections_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForLoopControlRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> condition_block_names;
    std::vector<std::string> body_block_names;
    std::vector<std::string> continue_block_names;
    std::vector<std::string> exit_block_names;
    std::vector<std::string> index_names;
    std::vector<std::string> next_index_names;
    std::vector<std::string> bounds_check_names;
    bool render_metadata_available = false;
    bool all_control_flow_names_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForElementAddressRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> element_llvm_type_names;
    std::vector<std::string> data_pointer_names;
    std::vector<std::string> index_names;
    std::vector<std::string> element_address_names;
    bool render_metadata_available = false;
    bool all_element_address_inputs_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForElementLoadRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> element_llvm_type_names;
    std::vector<std::string> element_address_names;
    std::vector<std::string> item_value_names;
    bool render_metadata_available = false;
    bool all_element_load_inputs_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForLoopContinueRenderState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> continue_block_names;
    std::vector<std::string> condition_block_names;
    std::vector<std::string> index_names;
    std::vector<std::string> next_index_names;
    bool render_metadata_available = false;
    bool all_loop_continue_inputs_ready = false;
    std::size_t render_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForLoopRenderSequenceState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> body_block_names;
    bool sequence_metadata_available = false;
    bool all_body_blocks_ready = false;
    std::size_t sequence_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForLoopExitCleanupState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> exit_block_names;
    std::vector<std::string> loop_entry_cleanup_owner_names;
    std::vector<std::string> loop_exit_cleanup_owner_names;
    std::vector<std::string> cleanup_resumption_operation_names;
    bool cleanup_metadata_available = false;
    bool all_cleanup_resumptions_ready = false;
    std::size_t cleanup_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForCleanupTransitionState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    std::vector<std::string> acquisition_source_owner_names;
    std::vector<std::string> acquisition_target_owner_names;
    std::vector<std::string> acquisition_operation_names;
    std::vector<std::string> resumption_source_owner_names;
    std::vector<std::string> resumption_target_owner_names;
    std::vector<std::string> resumption_operation_names;
    bool transition_metadata_available = false;
    bool all_transitions_paired = false;
    std::size_t transition_count = 0;
};

struct ComputedDynamicArrayForProductionEmissionGateState {
    std::vector<std::string> cleanup_owner_names;
    bool gate_metadata_available = false;
    bool all_ownership_ready = false;
    bool all_loop_render_ready = false;
    bool all_loop_cleanup_ownership_ready = false;
    bool all_function_cleanup_resumption_ready = false;
    bool all_exit_cleanup_ready = false;
    bool all_production_sequences_planned = false;
    bool any_production_emission_enabled = false;
    std::size_t gate_count = 0;
    std::size_t rendered_ir_snippet_count = 0;
};

struct ComputedDynamicArrayForProductionReadiness {
    bool gate_ready = false;
    bool sequence_ready = false;
    bool inserted_cleanup_transition_ready = false;
    bool inserted_cleanup_state_verification_ready = false;
    bool gate_sequence_counts_match = false;
    bool gate_sequence_snippets_match = false;
    bool sequence_transition_counts_match = false;
    bool transition_verification_counts_match = false;
    bool cleanup_owners_match = false;
    bool production_emission_enabled = false;
};

struct ConsumedDescriptorFinalizationState {
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> descriptor_storage_names;
    bool all_ready = false;
    std::size_t computed_descriptor_plan_count = 0;
    std::size_t emitted_finalization_plan_count = 0;
    std::size_t ready_plan_count = 0;
    std::size_t blocked_plan_count = 0;
};

struct ComputedConsumedCleanupDescriptorModelState {
    std::vector<std::string> enclosing_function_names;
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> descriptor_storage_names;
    std::vector<std::string> cleanup_operation_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> element_source_type_names;
    bool all_finalization_ready = false;
    std::size_t descriptor_model_count = 0;
    std::size_t ready_model_count = 0;
    std::size_t blocked_model_count = 0;
};

struct ComputedConsumedCleanupDescriptorState {
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> descriptor_storage_names;
    bool all_finalized = false;
    std::size_t descriptor_count = 0;
    std::size_t structured_proof_count = 0;
    std::size_t ir_fallback_proof_count = 0;
};

struct ComputedInsertedCleanupCallState {
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> data_pointer_names;
    std::vector<std::string> capacity_names;
    bool all_inserted = false;
    std::size_t call_count = 0;
    std::size_t structured_proof_count = 0;
    std::size_t ir_fallback_proof_count = 0;
};

struct ComputedCleanupCallInsertionGateState {
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> cleanup_operation_names;
    std::vector<std::string> cleanup_calls_blocked_reasons;
    bool all_state_verified = false;
    bool all_operands_proven = false;
    bool all_cleanup_calls_authorized = false;
    bool all_ready = false;
    std::size_t gate_count = 0;
    std::size_t ready_count = 0;
    std::size_t blocked_count = 0;
    std::size_t cleanup_call_blocker_count = 0;
};

struct ComputedCleanupCallInsertionCapabilityState {
    bool cleanup_call_authorization_enabled = false;
    bool cleanup_call_insertion_enabled = false;
    bool enabled = false;
};

struct ComputedCleanupCallPlanRenderState {
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> cleanup_operation_names;
    std::vector<std::string> data_pointer_names;
    std::vector<std::string> element_size_bytes;
    std::vector<std::string> capacity_names;
    bool all_state_verified = false;
    bool all_operands_proven = false;
    bool all_cleanup_calls_enabled = false;
    bool all_renderable = false;
    std::size_t plan_count = 0;
    std::size_t render_count = 0;
    std::size_t planned_count = 0;
    std::size_t renderable_count = 0;
};

struct ComputedCleanupCallEmissionGateState {
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> acquire_operation_names;
    std::vector<std::string> resume_operation_names;
    bool all_state_verified = false;
    bool all_cleanup_calls_enabled = false;
    bool all_ready = false;
    std::size_t gate_count = 0;
    std::size_t ready_count = 0;
    std::size_t blocked_count = 0;
};

struct ComputedCleanupProofSummaryState {
    std::size_t cleanup_proof_model_count = 0;
    std::size_t verified_inserted_cleanup_pair_count = 0;
    std::size_t structured_inserted_cleanup_handoff_count = 0;
    std::size_t structured_inserted_cleanup_handoff_use_count = 0;
    std::size_t ir_inserted_cleanup_handoff_fallback_count = 0;
    std::size_t structured_cleanup_operand_count = 0;
    std::size_t structured_cleanup_operand_use_count = 0;
    std::size_t ir_cleanup_operand_fallback_count = 0;
    std::size_t structured_inserted_cleanup_call_count = 0;
    std::size_t ir_inserted_cleanup_call_fallback_count = 0;
    std::size_t structured_consumed_cleanup_descriptor_count = 0;
    std::size_t ir_consumed_cleanup_descriptor_fallback_count = 0;
};

struct AggregateProjectionAccessPlanState {
    std::vector<std::string> function_symbol_names;
    std::vector<lowering::AggregateProjectionAccessIntent> intents;
    std::vector<lowering::AggregateProjectionAccessStatus> statuses;
    std::vector<std::string> binding_names;
    std::vector<std::string> source_type_names;
    std::vector<std::string> diagnostics;
    std::vector<bool> receiver_projections;
    bool access_plans_available = false;
    std::size_t plan_count = 0;
    std::size_t allowed_count = 0;
    std::size_t blocked_count = 0;
    std::size_t receiver_projection_count = 0;
};

struct ComputedInsertedCleanupHandoffState {
    std::vector<std::string> cleanup_owner_names;
    std::vector<std::string> acquire_operation_names;
    std::vector<std::string> resume_operation_names;
    std::vector<std::string> cleanup_calls_blocked_reasons;
    bool from_metadata = false;
    bool all_paired = false;
    bool all_cleanup_calls_enabled = false;
    std::size_t transition_count = 0;
    std::size_t verification_count = 0;
    std::size_t paired_count = 0;
    std::size_t blocked_count = 0;
    std::size_t cleanup_call_blocker_count = 0;
};

struct ComputedInsertedCleanupTransitionState {
    std::vector<std::string> acquire_source_owner_names;
    std::vector<std::string> acquire_target_owner_names;
    std::vector<std::string> resume_source_owner_names;
    std::vector<std::string> resume_target_owner_names;
    std::vector<std::string> acquire_operation_names;
    std::vector<std::string> resume_operation_names;
    bool from_metadata = false;
    bool transitions_available = false;
    bool all_cleanup_calls_enabled = false;
    std::size_t transition_count = 0;
};

struct ComputedInsertedCleanupStateVerificationState {
    std::vector<std::string> acquire_operation_names;
    std::vector<std::string> resume_operation_names;
    std::vector<std::string> acquire_source_owner_names;
    std::vector<std::string> acquire_target_owner_names;
    std::vector<std::string> resume_source_owner_names;
    std::vector<std::string> resume_target_owner_names;
    std::vector<std::string> blocked_reasons;
    bool from_metadata = false;
    bool all_paired = false;
    bool all_cleanup_calls_enabled = false;
    std::size_t verification_count = 0;
    std::size_t paired_count = 0;
    std::size_t blocked_count = 0;
};

auto dynamic_array_cleanup_production_ready(
    DynamicArrayCleanupProductionReadiness const& readiness
) -> bool;

auto format_dynamic_array_cleanup_production_readiness(
    DynamicArrayCleanupProductionReadiness const& readiness
) -> std::string;

auto plan_computed_dynamic_array_for_production_readiness(
    ComputedDynamicArrayForProductionEmissionGateState const& gate_state,
    ComputedDynamicArrayForProductionSequenceState const& sequence_state,
    ComputedInsertedCleanupTransitionState const& inserted_transition_state,
    ComputedInsertedCleanupStateVerificationState const& inserted_verification_state,
    ComputedCleanupCallInsertionCapabilityState const& insertion_capability_state
) -> ComputedDynamicArrayForProductionReadiness;

auto computed_dynamic_array_for_production_ready(
    ComputedDynamicArrayForProductionReadiness const& readiness
) -> bool;

struct CompilePipelineOptions {
    std::vector<semantics::DropImplementation> test_only_semantic_drop_implementations;
    std::vector<semantics::DropImplementationCandidate> test_only_semantic_drop_implementation_candidates;
    std::vector<semantics::DropLoweringAuthorization> test_only_semantic_drop_lowering_authorizations;
    std::vector<lowering::TestOnlyDynamicArrayConstructionRequest> test_only_dynamic_array_construction_requests;
    bool test_only_enable_source_drop_lowering = false;
    bool source_drop_lowering_enabled = false;
    bool test_only_derive_dynamic_array_cleanup_from_semantics = false;
    bool dynamic_array_descriptor_cleanup_planning_enabled = false;
    bool test_only_enable_dynamic_array_parameter_descriptors = false;
    bool dynamic_array_parameter_descriptor_audit_bindings_enabled = false;
    bool test_only_emit_bound_dynamic_array_parameter_cleanups = false;
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
    bool test_only_authorize_computed_dynamic_array_cleanup_calls = false;
    bool test_only_insert_computed_dynamic_array_cleanup_calls = false;
    bool collect_aggregate_projection_access_metadata = false;
    bool test_only_suppress_computed_dynamic_array_cleanup_handoff_metadata = false;
    bool test_only_suppress_computed_dynamic_array_cleanup_operand_metadata = false;
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

struct CompilePipelineResult {
    std::optional<source::SourceFile> source_file;
    syntax::ParseResult parse_result;
    semantics::SemanticAnalysisResult semantic_result;
    std::string ir_text;
    std::string object_bytes;
    std::vector<std::string> semantic_dynamic_array_descriptor_origin_report;
    std::vector<std::string> semantic_planned_drop_report;
    std::vector<std::string> semantic_drop_implementation_report;
    std::vector<std::string> semantic_drop_resolution_report;
    std::vector<std::string> semantic_drop_diagnostic_report;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;
    std::vector<std::string> semantic_drop_lowering_authorization_report;
    std::vector<std::string> semantic_drop_resolution_summary_report;
    DynamicArrayDescriptorCleanupPlanState dynamic_array_descriptor_cleanup_plan_state;
    DynamicArrayConstructionPlanState dynamic_array_construction_plan_state;
    DynamicArrayRuntimeRequestState dynamic_array_runtime_request_state;
    DynamicArrayAllocationCallEmissionState dynamic_array_allocation_call_emission_state;
    DynamicArrayCleanupObligationState dynamic_array_cleanup_obligation_state;
    DynamicArrayCleanupSequencePlanState dynamic_array_cleanup_sequence_plan_state;
    DynamicArrayCleanupSequenceVerificationState dynamic_array_cleanup_sequence_verification_state;
    bool dynamic_array_cleanup_sequence_verification_passed = false;
    bool dynamic_array_cleanup_capability_proven = false;
    std::vector<std::string> dynamic_array_cleanup_missing_element_drop_pairs;
    DynamicArrayCleanupEmissionCapabilityState dynamic_array_cleanup_emission_capability_state;
    DynamicArrayCleanupAvailability dynamic_array_cleanup_availability;
    DynamicArrayCleanupObligationState emitted_dynamic_array_cleanup_obligation_state;
    DynamicArrayCleanupSequencePlanState emitted_dynamic_array_cleanup_sequence_plan_state;
    DynamicArrayCleanupSequenceVerificationState emitted_dynamic_array_cleanup_sequence_verification_state;
    ComputedDynamicArrayForDescriptorRenderState computed_dynamic_array_for_descriptor_render_state;
    ComputedDynamicArrayForLoopControlRenderState computed_dynamic_array_for_loop_control_render_state;
    ComputedDynamicArrayForElementAddressRenderState computed_dynamic_array_for_element_address_render_state;
    ComputedDynamicArrayForElementLoadRenderState computed_dynamic_array_for_element_load_render_state;
    ComputedDynamicArrayForLoopContinueRenderState computed_dynamic_array_for_loop_continue_render_state;
    ComputedDynamicArrayForLoopRenderSequenceState computed_dynamic_array_for_loop_render_sequence_state;
    ComputedDynamicArrayForLoopExitCleanupState computed_dynamic_array_for_loop_exit_cleanup_state;
    ComputedDynamicArrayForCleanupTransitionState computed_dynamic_array_for_cleanup_transition_state;
    ComputedInsertedCleanupTransitionState computed_dynamic_array_for_inserted_cleanup_transition_state;
    ComputedInsertedCleanupStateVerificationState
        computed_dynamic_array_for_inserted_cleanup_state_verification_state;
    ComputedInsertedCleanupHandoffState computed_dynamic_array_for_inserted_cleanup_handoff_state;
    ComputedCleanupProofSummaryState computed_dynamic_array_for_cleanup_proof_summary_state;
    ComputedCleanupCallEmissionGateState computed_dynamic_array_for_cleanup_call_emission_gate_state;
    ComputedCleanupCallPlanRenderState computed_dynamic_array_for_cleanup_call_plan_render_state;
    ComputedCleanupCallInsertionGateState computed_dynamic_array_for_cleanup_call_insertion_gate_state;
    ComputedCleanupCallInsertionCapabilityState computed_dynamic_array_for_cleanup_call_insertion_capability_state;
    ComputedInsertedCleanupCallState computed_dynamic_array_for_inserted_cleanup_call_state;
    ConsumedDescriptorFinalizationState consumed_descriptor_finalization_state;
    ComputedConsumedCleanupDescriptorModelState computed_dynamic_array_for_consumed_cleanup_descriptor_model_state;
    ComputedConsumedCleanupDescriptorState computed_dynamic_array_for_consumed_cleanup_descriptor_state;
    AggregateProjectionAccessPlanState aggregate_projection_access_plan_state;
    ComputedDynamicArrayForProductionEmissionGateState computed_dynamic_array_for_production_emission_gate_state;
    ComputedDynamicArrayForProductionSequenceState computed_dynamic_array_for_production_sequence_state;
    ComputedDynamicArrayForProductionReadiness computed_dynamic_array_for_production_readiness;
    std::vector<std::string> computed_dynamic_array_for_production_sequence_module_ir;
    DynamicArrayCleanupProductionReadiness dynamic_array_cleanup_production_readiness;
    PlannedDropDeclarationState planned_drop_declaration_state;
    PlannedDropActionState planned_drop_action_state;
    DropCleanupAuthorizationState drop_cleanup_authorization_state;
    lowering::DropReadinessSnapshot drop_readiness_snapshot;
    lowering::DropReadinessSummary drop_readiness_summary;
    lowering::DropReadinessBlockerSummary drop_readiness_blocker_summary;
    std::vector<std::string> drop_readiness_blocker_report;
    std::vector<std::string> drop_readiness_source_correlation_report;
    std::vector<std::string> link_libraries;
    std::string error_text;

    auto has_errors() const -> bool;
};

class CompilePipeline {
public:
    auto analyze(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto analyze(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto emit_llvm(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto emit_llvm(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto emit_object(std::filesystem::path const& source_path) const -> CompilePipelineResult;
    auto emit_object(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
    auto collect_dynamic_array_cleanup_metadata(
        std::filesystem::path const& source_path,
        CompilePipelineOptions const& options
    ) const -> CompilePipelineResult;
};

}  // namespace orison::pipeline
