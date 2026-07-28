#include "lowering_emission_reports.hpp"

#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include "dynamic_array_cleanup_readiness.hpp"
#include "computed_cleanup_proof_model.hpp"

namespace orison::pipeline {

void populate_lowering_emission_reports(
    CompilePipelineResult& result,
    lowering::LlvmIrEmissionResult&& emission,
    CompilePipelineOptions const& options
) {
    result.ir_text = std::move(emission.ir_text);
    auto const cleanup_proof_model = build_computed_cleanup_proof_model(
        result.ir_text,
        emission.computed_dynamic_array_inserted_cleanup_handoffs,
        emission.computed_dynamic_array_cleanup_call_operands
    );
    result.dynamic_array_construction_plan_report =
        emission.dynamic_array_construction_plan_report();
    result.dynamic_array_runtime_request_report =
        emission.dynamic_array_runtime_request_report();
    result.dynamic_array_allocation_call_ir =
        std::move(emission.dynamic_array_allocation_call_ir);
    result.dynamic_array_descriptor_cleanup_plan_report =
        emission.dynamic_array_descriptor_cleanup_plan_report();
    result.dynamic_array_cleanup_obligation_report =
        emission.dynamic_array_cleanup_obligation_report();
    result.dynamic_array_cleanup_sequence_plan_report =
        emission.dynamic_array_cleanup_sequence_plan_report();
    result.dynamic_array_cleanup_sequence_verification_report =
        emission.dynamic_array_cleanup_sequence_verification_report();
    result.dynamic_array_cleanup_sequence_verification_passed =
        !emission.dynamic_array_cleanup_sequence_verifications.empty() &&
        lowering::dynamic_array_cleanup_sequence_verification_report_passed(
            emission.dynamic_array_cleanup_sequence_verifications
        );
    result.dynamic_array_cleanup_emission_gate_report =
        emission.dynamic_array_cleanup_emission_gate_report();
    if (emission.dynamic_array_cleanup_emission_capability.has_value()) {
        result.dynamic_array_cleanup_capability_proven =
            lowering::dynamic_array_cleanup_emission_capability_proven(
                *emission.dynamic_array_cleanup_emission_capability
            );
        result.dynamic_array_cleanup_missing_element_drop_pairs =
            emission.dynamic_array_cleanup_emission_capability->missing_element_drop_pairs;
    }
    result.dynamic_array_cleanup_emission_capability_report =
        emission.dynamic_array_cleanup_emission_capability_report();
    result.dynamic_array_cleanup_availability = DynamicArrayCleanupAvailability {
        .missing_element_drop_pairs = result.dynamic_array_cleanup_missing_element_drop_pairs,
        .descriptor_origins_available = !result.semantic_result.dynamic_array_descriptor_origins.empty(),
        .descriptor_cleanup_plans_available = !emission.dynamic_array_descriptor_cleanup_plans.empty(),
        .cleanup_obligations_available = !emission.dynamic_array_cleanup_obligations.empty(),
        .sequence_verification_available = !emission.dynamic_array_cleanup_sequence_verifications.empty(),
        .sequence_verification_passed = result.dynamic_array_cleanup_sequence_verification_passed,
        .cleanup_capability_proven = result.dynamic_array_cleanup_capability_proven,
    };
    result.emitted_dynamic_array_cleanup_obligation_report =
        std::move(emission.emitted_dynamic_array_cleanup_obligation_report);
    result.emitted_dynamic_array_cleanup_sequence_plan_report =
        std::move(emission.emitted_dynamic_array_cleanup_sequence_plan_report);
    result.emitted_dynamic_array_cleanup_sequence_verification_report =
        std::move(emission.emitted_dynamic_array_cleanup_sequence_verification_report);
    result.emitted_dynamic_array_cleanup_emission_gate_report =
        std::move(emission.emitted_dynamic_array_cleanup_emission_gate_report);
    result.emitted_dynamic_array_cleanup_emission_capability_report =
        std::move(emission.emitted_dynamic_array_cleanup_emission_capability_report);
    result.computed_dynamic_array_for_descriptor_render_report =
        emission.computed_dynamic_array_for_descriptor_render_report();
    result.computed_dynamic_array_for_loop_control_render_report =
        emission.computed_dynamic_array_for_loop_control_render_report();
    result.computed_dynamic_array_for_element_address_render_report =
        emission.computed_dynamic_array_for_element_address_render_report();
    result.computed_dynamic_array_for_element_load_render_report =
        emission.computed_dynamic_array_for_element_load_render_report();
    result.computed_dynamic_array_for_loop_continue_render_report =
        emission.computed_dynamic_array_for_loop_continue_render_report();
    result.computed_dynamic_array_for_loop_render_sequence_report =
        emission.computed_dynamic_array_for_loop_render_sequence_report();
    result.computed_dynamic_array_for_loop_exit_cleanup_report =
        emission.computed_dynamic_array_for_loop_exit_cleanup_report();
    result.computed_dynamic_array_for_cleanup_transition_report =
        emission.computed_dynamic_array_for_cleanup_transition_report();
    result.computed_dynamic_array_for_inserted_cleanup_transition_report =
        cleanup_proof_model.reports.inserted_cleanup_transition_report;
    result.computed_dynamic_array_for_inserted_cleanup_state_verification_report =
        cleanup_proof_model.reports.inserted_cleanup_state_verification_report;
    result.computed_dynamic_array_for_cleanup_proof_model_count =
        cleanup_proof_model.summary.cleanup_proof_model_count;
    result.computed_dynamic_array_for_verified_inserted_cleanup_pair_count =
        cleanup_proof_model.summary.verified_inserted_cleanup_pair_count;
    result.computed_dynamic_array_for_structured_inserted_cleanup_handoff_count =
        cleanup_proof_model.summary.structured_inserted_cleanup_handoff_count;
    result.computed_dynamic_array_for_structured_inserted_cleanup_handoff_use_count =
        cleanup_proof_model.summary.structured_inserted_cleanup_handoff_use_count;
    result.computed_dynamic_array_for_ir_inserted_cleanup_handoff_fallback_count =
        cleanup_proof_model.summary.ir_inserted_cleanup_handoff_fallback_count;
    result.computed_dynamic_array_for_structured_cleanup_operand_count =
        cleanup_proof_model.summary.structured_cleanup_operand_count;
    result.computed_dynamic_array_for_structured_cleanup_operand_use_count =
        cleanup_proof_model.summary.structured_cleanup_operand_use_count;
    result.computed_dynamic_array_for_ir_cleanup_operand_fallback_count =
        cleanup_proof_model.summary.ir_cleanup_operand_fallback_count;
    result.computed_dynamic_array_for_structured_inserted_cleanup_call_count =
        cleanup_proof_model.summary.structured_inserted_cleanup_call_count;
    result.computed_dynamic_array_for_structured_consumed_cleanup_descriptor_count =
        cleanup_proof_model.summary.structured_consumed_cleanup_descriptor_count;
    result.computed_dynamic_array_for_ir_inserted_cleanup_call_fallback_count =
        cleanup_proof_model.summary.ir_inserted_cleanup_call_fallback_count;
    result.computed_dynamic_array_for_ir_consumed_cleanup_descriptor_fallback_count =
        cleanup_proof_model.summary.ir_consumed_cleanup_descriptor_fallback_count;
    result.computed_dynamic_array_for_cleanup_call_emission_gate_report =
        cleanup_proof_model.reports.cleanup_call_emission_gate_report;
    result.computed_dynamic_array_for_cleanup_call_plan_report =
        cleanup_proof_model.reports.cleanup_call_plan_report;
    result.computed_dynamic_array_for_cleanup_call_render_report =
        cleanup_proof_model.reports.cleanup_call_render_report;
    result.computed_dynamic_array_for_cleanup_call_insertion_gate_report =
        cleanup_proof_model.reports.cleanup_call_insertion_gate_report;
    result.computed_dynamic_array_for_inserted_cleanup_call_report =
        cleanup_proof_model.reports.inserted_cleanup_call_report;
    result.consumed_descriptor_finalization_plan_report =
        emission.consumed_descriptor_finalization_plan_report();
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_model_report =
        emission.computed_dynamic_array_for_consumed_cleanup_descriptor_model_report();
    result.computed_dynamic_array_for_consumed_cleanup_descriptor_report =
        cleanup_proof_model.reports.consumed_cleanup_descriptor_report;
    result.computed_dynamic_array_for_production_emission_gate_report =
        emission.computed_dynamic_array_for_production_emission_gate_report();
    result.computed_dynamic_array_for_production_sequence_report =
        emission.computed_dynamic_array_for_production_sequence_report();
    result.test_only_computed_dynamic_array_for_production_sequence_module_ir =
        std::move(emission.test_only_computed_dynamic_array_for_production_sequence_module_ir);
    result.dynamic_array_cleanup_production_readiness =
        plan_dynamic_array_cleanup_production_readiness(result, options);
    result.dynamic_array_cleanup_production_readiness_report = {
        format_dynamic_array_cleanup_production_readiness(result.dynamic_array_cleanup_production_readiness),
    };
    result.planned_drop_report = emission.planned_drop_report();
    result.emitted_drop_declaration_report =
        emission.emitted_drop_declaration_report();
    result.planned_drop_action_report =
        emission.planned_drop_action_report();
    result.drop_cleanup_authorization_report =
        emission.drop_cleanup_authorization_report();
    result.drop_readiness_snapshot = emission.drop_readiness_snapshot();
    result.drop_readiness_snapshot_report =
        emission.drop_readiness_snapshot_report();
    result.drop_readiness_summary = emission.drop_readiness_summary();
    result.drop_readiness_summary_report =
        emission.drop_readiness_summary_report();
    result.drop_readiness_relation_report =
        emission.drop_readiness_relation_report();
    result.drop_readiness_blocker_summary =
        lowering::summarize_drop_readiness_blockers(result.drop_readiness_snapshot);
    result.drop_readiness_blocker_report =
        lowering::format_drop_readiness_blocker_report(result.drop_readiness_blocker_summary);
    result.drop_readiness_source_correlation_report =
        format_drop_readiness_source_correlation_report(result.drop_readiness_snapshot);
    result.semantic_drop_lowering_authorizations = std::move(emission.semantic_drop_lowering_authorizations);
}

}  // namespace orison::pipeline
