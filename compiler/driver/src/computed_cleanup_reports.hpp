#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

#include <string>
#include <vector>

namespace orison::driver {

auto dynamic_array_descriptor_cleanup_plan_state_report(
    pipeline::DynamicArrayDescriptorCleanupPlanState const& state
) -> std::vector<std::string>;

auto dynamic_array_descriptor_lifetime_plan_state_report(
    pipeline::DynamicArrayDescriptorLifetimePlanState const& state
) -> std::vector<std::string>;

auto dynamic_array_cleanup_obligation_state_report(
    pipeline::DynamicArrayCleanupObligationState const& state
) -> std::vector<std::string>;

auto dynamic_array_cleanup_sequence_plan_state_report(
    pipeline::DynamicArrayCleanupSequencePlanState const& state
) -> std::vector<std::string>;

auto dynamic_array_cleanup_sequence_verification_state_report(
    pipeline::DynamicArrayCleanupSequenceVerificationState const& state
) -> std::vector<std::string>;

auto dynamic_array_cleanup_emission_gate_state_report(
    pipeline::DynamicArrayCleanupSequenceVerificationState const& state
) -> std::vector<std::string>;

auto computed_cleanup_call_insertion_capability_report(
    pipeline::ComputedCleanupCallInsertionCapabilityState const& state
) -> std::vector<std::string>;

auto computed_cleanup_call_insertion_readiness_report(
    pipeline::ComputedCleanupCallInsertionGateState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_descriptor_render_state_report(
    pipeline::ComputedDynamicArrayForDescriptorRenderState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_loop_control_render_state_report(
    pipeline::ComputedDynamicArrayForLoopControlRenderState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_element_address_render_state_report(
    pipeline::ComputedDynamicArrayForElementAddressRenderState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_element_load_render_state_report(
    pipeline::ComputedDynamicArrayForElementLoadRenderState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_loop_continue_render_state_report(
    pipeline::ComputedDynamicArrayForLoopContinueRenderState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_loop_render_sequence_state_report(
    pipeline::ComputedDynamicArrayForLoopRenderSequenceState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_loop_exit_cleanup_state_report(
    pipeline::ComputedDynamicArrayForLoopExitCleanupState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_cleanup_transition_state_report(
    pipeline::ComputedDynamicArrayForCleanupTransitionState const& state
) -> std::vector<std::string>;

auto computed_inserted_cleanup_handoff_state_report(
    pipeline::ComputedInsertedCleanupHandoffState const& state
) -> std::vector<std::string>;

auto computed_inserted_cleanup_state_verification_report(
    pipeline::ComputedInsertedCleanupStateVerificationState const& state
) -> std::vector<std::string>;

auto computed_cleanup_call_blocker_summary_report(
    pipeline::ComputedInsertedCleanupHandoffState const& state
) -> std::vector<std::string>;

auto computed_cleanup_call_emission_gate_state_report(
    pipeline::ComputedCleanupCallEmissionGateState const& state
) -> std::vector<std::string>;

auto computed_cleanup_call_plan_state_report(
    pipeline::ComputedCleanupCallPlanRenderState const& state
) -> std::vector<std::string>;

auto computed_cleanup_call_render_state_report(
    pipeline::ComputedCleanupCallPlanRenderState const& state
) -> std::vector<std::string>;

auto computed_inserted_cleanup_call_state_report(
    pipeline::ComputedInsertedCleanupCallState const& state
) -> std::vector<std::string>;

auto consumed_descriptor_finalization_state_report(
    pipeline::ConsumedDescriptorFinalizationState const& state
) -> std::vector<std::string>;

auto computed_consumed_cleanup_descriptor_model_state_report(
    pipeline::ComputedConsumedCleanupDescriptorModelState const& state
) -> std::vector<std::string>;

auto computed_consumed_cleanup_descriptor_state_report(
    pipeline::ComputedConsumedCleanupDescriptorState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_production_emission_gate_state_report(
    pipeline::ComputedDynamicArrayForProductionEmissionGateState const& state
) -> std::vector<std::string>;

auto computed_dynamic_array_for_production_sequence_state_report(
    pipeline::ComputedDynamicArrayForProductionSequenceState const& state
) -> std::vector<std::string>;

auto dynamic_array_cleanup_production_readiness_state_report(
    pipeline::DynamicArrayCleanupProductionReadiness const& state
) -> std::vector<std::string>;

auto dynamic_array_cleanup_emission_capability_state_report(
    pipeline::DynamicArrayCleanupEmissionCapabilityState const& state
) -> std::vector<std::string>;

auto computed_cleanup_proof_summary_state_report(
    pipeline::ComputedCleanupProofSummaryState const& state
) -> std::vector<std::string>;

auto aggregate_projection_access_plan_state_report(
    pipeline::AggregateProjectionAccessPlanState const& state
) -> std::vector<std::string>;

}  // namespace orison::driver
