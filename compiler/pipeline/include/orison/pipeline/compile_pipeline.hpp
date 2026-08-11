#pragma once

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/aggregate_projection_access_plan.hpp"
#include "orison/lowering/dynamic_array_cleanup_metadata.hpp"
#include "orison/lowering/lowering_options.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/pipeline/computed_dynamic_array_cleanup_states.hpp"
#include "orison/pipeline/computed_dynamic_array_production_sequence.hpp"
#include "orison/pipeline/computed_dynamic_array_render_states.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_cfg_rewrite.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_composition_failures.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_emission.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_function_ir_rewrite_candidates.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_module_ir_insertion.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_module_ir_rewrite_candidates.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_production_readiness.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_ranges.hpp"
#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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

struct DynamicArrayAllocationCallIrArtifactState {
    std::vector<std::string> rendered_ir_snippets;
};

struct DynamicArrayAllocationCallEmissionState {
    DynamicArrayAllocationCallIrArtifactState ir_artifact_state;
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

struct SemanticDropImplementationDiscovery {
    semantics::DropImplementation implementation;
    std::string discovery_name;
};

struct SemanticDropState {
    std::vector<SemanticDropImplementationDiscovery> discovered_implementations;
    std::vector<semantics::DropImplementationResolutionSummary> resolution_summaries;
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
    bool runtime_indexed_constructor_move_enabled = false;
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

struct CompilePipelineResult {
    std::optional<source::SourceFile> source_file;
    syntax::ParseResult parse_result;
    semantics::SemanticAnalysisResult semantic_result;
    std::string ir_text;
    std::string object_bytes;
    SemanticDropState semantic_drop_state;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;
    DynamicArrayDescriptorCleanupPlanState dynamic_array_descriptor_cleanup_plan_state;
    DynamicArrayConstructionPlanState dynamic_array_construction_plan_state;
    DynamicArrayRuntimeRequestState dynamic_array_runtime_request_state;
    DynamicArrayAllocationCallEmissionState dynamic_array_allocation_call_emission_state;
    DynamicArrayCleanupObligationState dynamic_array_cleanup_obligation_state;
    DynamicArrayCleanupSequencePlanState dynamic_array_cleanup_sequence_plan_state;
    DynamicArrayCleanupSequenceVerificationState dynamic_array_cleanup_sequence_verification_state;
    bool dynamic_array_cleanup_sequence_verification_passed = false;
    bool dynamic_array_cleanup_capability_proven = false;
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
    ComputedDynamicArrayForProductionSequenceModuleIrArtifactState
        computed_dynamic_array_for_production_sequence_module_ir_artifact_state;
    DynamicArrayCleanupProductionReadiness dynamic_array_cleanup_production_readiness;
    PlannedDropDeclarationState planned_drop_declaration_state;
    PlannedDropActionState planned_drop_action_state;
    DropCleanupAuthorizationState drop_cleanup_authorization_state;
    lowering::DropReadinessSnapshot drop_readiness_snapshot;
    lowering::DropReadinessSummary drop_readiness_summary;
    lowering::DropReadinessBlockerSummary drop_readiness_blocker_summary;
    RuntimeIndexedCleanupCapabilityState runtime_indexed_cleanup_capability_state;
    RuntimeIndexedCleanupEmissionPlanState runtime_indexed_cleanup_emission_plan_state;
    RuntimeIndexedCleanupIrRenderState runtime_indexed_cleanup_ir_render_state;
    RuntimeIndexedCleanupModuleIrArtifactState runtime_indexed_cleanup_module_ir_artifact_state;
    RuntimeIndexedCleanupModuleIrInsertionGateState runtime_indexed_cleanup_module_ir_insertion_gate_state;
    RuntimeIndexedCleanupModuleIrInsertionPreviewState runtime_indexed_cleanup_module_ir_insertion_preview_state;
    RuntimeIndexedCleanupModuleIrCandidateState runtime_indexed_cleanup_module_ir_candidate_state;
    RuntimeIndexedCleanupModuleIrCandidateVerificationState
        runtime_indexed_cleanup_module_ir_candidate_verification_state;
    RuntimeIndexedCleanupModuleIrMutationState
        runtime_indexed_cleanup_module_ir_mutation_state;
    RuntimeIndexedCleanupFunctionCfgRewritePlanState
        runtime_indexed_cleanup_function_cfg_rewrite_plan_state;
    RuntimeIndexedCleanupFunctionCfgRewriteVerificationState
        runtime_indexed_cleanup_function_cfg_rewrite_verification_state;
    RuntimeIndexedCleanupFunctionIrRewriteCandidateState
        runtime_indexed_cleanup_function_ir_rewrite_candidate_state;
    RuntimeIndexedCleanupFunctionIrRewriteCandidateVerificationState
        runtime_indexed_cleanup_function_ir_rewrite_candidate_verification_state;
    RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateState
        runtime_indexed_cleanup_function_ir_module_rewrite_candidate_state;
    RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState
        runtime_indexed_cleanup_function_ir_module_rewrite_candidate_verification_state;
    RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState
        runtime_indexed_cleanup_function_ir_module_rewrite_mutation_state;
    RuntimeIndexedCleanupModuleIrProductionReadinessState
        runtime_indexed_cleanup_module_ir_production_readiness_state;
    std::vector<std::string> runtime_indexed_cleanup_audit_lines;
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
