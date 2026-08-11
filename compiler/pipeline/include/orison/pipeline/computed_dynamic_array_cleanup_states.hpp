#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

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
    std::size_t production_cleanup_call_authorization_count = 0;
    std::size_t explicit_test_seam_cleanup_call_authorization_count = 0;
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

}  // namespace orison::pipeline
