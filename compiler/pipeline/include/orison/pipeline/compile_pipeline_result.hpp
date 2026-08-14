#pragma once

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/pipeline/aggregate_projection_access_pipeline_state.hpp"
#include "orison/pipeline/computed_dynamic_array_cleanup_states.hpp"
#include "orison/pipeline/computed_dynamic_array_production_sequence.hpp"
#include "orison/pipeline/computed_dynamic_array_render_states.hpp"
#include "orison/pipeline/dynamic_array_pipeline_states.hpp"
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

#include <optional>
#include <string>
#include <vector>

namespace orison::pipeline {

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
    RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState
        runtime_indexed_member_cleanup_function_ir_module_rewrite_mutation_state;
    RuntimeIndexedCleanupModuleIrProductionReadinessState
        runtime_indexed_cleanup_module_ir_production_readiness_state;
    std::vector<lowering::RuntimeIndexedMemberCleanupSiblingField>
        runtime_indexed_member_cleanup_sibling_fields;
    std::vector<std::string> runtime_indexed_cleanup_audit_lines;
    std::vector<std::string> link_libraries;
    std::string error_text;

    auto has_errors() const -> bool;
};

}  // namespace orison::pipeline
