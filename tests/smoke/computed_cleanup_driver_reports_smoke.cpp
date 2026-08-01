#include "computed_dynamic_array_audit_expectations.hpp"

#include "computed_cleanup_reports.hpp"

#include <cassert>

namespace {

namespace driver = orison::driver;
namespace pipeline = orison::pipeline;
namespace smoke = orison::tests::smoke;

void assert_computed_cleanup_capability_reports() {
    auto disabled = driver::computed_cleanup_call_insertion_capability_report(
        pipeline::ComputedCleanupCallInsertionCapabilityState {}
    );
    assert(disabled.size() == 1);
    assert(disabled.front() == smoke::computed_dynamic_array_cleanup_call_insertion_capability_disabled_report);

    auto enabled = driver::computed_cleanup_call_insertion_capability_report(
        pipeline::ComputedCleanupCallInsertionCapabilityState {
            .cleanup_call_authorization_enabled = true,
            .cleanup_call_insertion_enabled = true,
            .enabled = true,
        }
    );
    assert(enabled.size() == 1);
    assert(enabled.front() == smoke::computed_dynamic_array_cleanup_call_insertion_capability_enabled_report);
}

void assert_computed_cleanup_readiness_reports() {
    auto blocked = driver::computed_cleanup_call_insertion_readiness_report(
        pipeline::ComputedCleanupCallInsertionGateState {
            .cleanup_owner_names = {"items"},
            .cleanup_operation_names = {"items.computed_for.0.cleanup.resume.call"},
            .cleanup_calls_blocked_reasons = {"later owner use"},
            .all_state_verified = true,
            .all_operands_proven = true,
            .all_cleanup_calls_authorized = false,
            .all_ready = false,
            .gate_count = 1,
            .ready_count = 0,
            .blocked_count = 1,
            .cleanup_call_blocker_count = 1,
        }
    );
    assert(blocked.size() == 2);
    assert(blocked[0] == smoke::computed_dynamic_array_cleanup_call_insertion_readiness_blocked_report);
    assert(
        blocked[1].find(
            "cleanup-operation items.computed_for.0.cleanup.resume.call cleanup-blocked-reason later owner use"
        ) != std::string::npos
    );

    auto ready = driver::computed_cleanup_call_insertion_readiness_report(
        pipeline::ComputedCleanupCallInsertionGateState {
            .cleanup_owner_names = {"items"},
            .cleanup_operation_names = {"items.computed_for.0.cleanup.resume.call"},
            .all_state_verified = true,
            .all_operands_proven = true,
            .all_cleanup_calls_authorized = true,
            .all_ready = true,
            .gate_count = 1,
            .ready_count = 1,
            .blocked_count = 0,
        }
    );
    assert(ready.size() == 2);
    assert(ready[0] == smoke::computed_dynamic_array_cleanup_call_insertion_readiness_ready_report);
    assert(ready[1] == smoke::computed_dynamic_array_cleanup_call_insertion_readiness_detail_report);
}

void assert_computed_inserted_cleanup_handoff_reports() {
    auto empty = driver::computed_inserted_cleanup_handoff_state_report(
        pipeline::ComputedInsertedCleanupHandoffState {}
    );
    assert(empty.size() == 1);
    assert(empty.front() == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_empty_report);
    assert(
        driver::computed_cleanup_call_blocker_summary_report(
            pipeline::ComputedInsertedCleanupHandoffState {}
        ).empty()
    );

    auto paired_disabled_state = pipeline::ComputedInsertedCleanupHandoffState {
        .cleanup_owner_names = {"items"},
        .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
        .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
        .cleanup_calls_blocked_reasons = {"later owner use"},
        .from_metadata = true,
        .all_paired = true,
        .all_cleanup_calls_enabled = false,
        .transition_count = 1,
        .verification_count = 1,
        .paired_count = 1,
        .blocked_count = 0,
        .cleanup_call_blocker_count = 1,
    };
    auto paired_disabled = driver::computed_inserted_cleanup_handoff_state_report(paired_disabled_state);
    assert(paired_disabled.size() == 2);
    assert(paired_disabled[0] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_paired_disabled_report);
    assert(
        paired_disabled[1].find(
            "acquire items.computed_for.0.cleanup.acquire resume items.computed_for.0.cleanup.resume "
            "cleanup-blocked-reason later owner use"
        ) != std::string::npos
    );
    auto paired_disabled_blockers =
        driver::computed_cleanup_call_blocker_summary_report(paired_disabled_state);
    assert(paired_disabled_blockers.size() == 1);
    assert(
        paired_disabled_blockers.front() ==
        "computed DynamicArray cleanup call blockers blocked cleanup-blockers 1 "
        "blocker-reasons [later owner use] (metadata only)"
    );

    auto paired_enabled = driver::computed_inserted_cleanup_handoff_state_report(
        pipeline::ComputedInsertedCleanupHandoffState {
            .cleanup_owner_names = {"items"},
            .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
            .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
            .from_metadata = true,
            .all_paired = true,
            .all_cleanup_calls_enabled = true,
            .transition_count = 1,
            .verification_count = 1,
            .paired_count = 1,
            .blocked_count = 0,
        }
    );
    assert(paired_enabled.size() == 2);
    assert(paired_enabled[0] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_paired_enabled_report);
    assert(paired_enabled[1] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_detail_report);
}

void assert_computed_cleanup_verification_and_emission_gate_reports() {
    auto blocked_verification = driver::computed_inserted_cleanup_state_verification_report(
        pipeline::ComputedInsertedCleanupStateVerificationState {
            .blocked_reasons = {"missing cleanup acquisition"},
            .from_metadata = true,
            .all_paired = false,
            .all_cleanup_calls_enabled = false,
            .verification_count = 1,
            .paired_count = 0,
            .blocked_count = 1,
        }
    );
    assert(blocked_verification.size() == 2);
    assert(
        blocked_verification[0] ==
        "computed DynamicArray inserted cleanup state verification blocked verifications 1 paired 0 blocked 1 "
        "[metadata-backed] [handoff blocked] [cleanup calls disabled] (inserted IR)"
    );
    assert(
        blocked_verification[1] ==
        "computed DynamicArray inserted cleanup state verification detail owner <unknown> blocked-reason "
        "missing cleanup acquisition (inserted IR)"
    );

    auto paired_verification = driver::computed_inserted_cleanup_state_verification_report(
        pipeline::ComputedInsertedCleanupStateVerificationState {
            .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
            .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
            .acquire_source_owner_names = {"items"},
            .acquire_target_owner_names = {"items.loop.entry"},
            .resume_source_owner_names = {"items.loop.entry"},
            .resume_target_owner_names = {"items"},
            .from_metadata = true,
            .all_paired = true,
            .all_cleanup_calls_enabled = true,
            .verification_count = 1,
            .paired_count = 1,
            .blocked_count = 0,
        }
    );
    assert(paired_verification.size() == 2);
    assert(
        paired_verification[0] ==
        "computed DynamicArray inserted cleanup state verification paired verifications 1 paired 1 blocked 0 "
        "[metadata-backed] [handoff paired] [cleanup calls enabled] (inserted IR)"
    );
    assert(
        paired_verification[1] ==
        "computed DynamicArray inserted cleanup state verification detail owner items acquire "
        "items.computed_for.0.cleanup.acquire resume items.computed_for.0.cleanup.resume acquire-from items "
        "acquire-to items.loop.entry resume-from items.loop.entry resume-to items (inserted IR)"
    );

    auto blocked_gate = driver::computed_cleanup_call_emission_gate_state_report(
        pipeline::ComputedCleanupCallEmissionGateState {
            .cleanup_owner_names = {"items"},
            .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
            .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
            .all_state_verified = true,
            .all_cleanup_calls_enabled = false,
            .all_ready = false,
            .gate_count = 1,
            .ready_count = 0,
            .blocked_count = 1,
        }
    );
    assert(blocked_gate.size() == 2);
    assert(
        blocked_gate[0] ==
        "computed DynamicArray cleanup call emission gate blocked gates 1 ready 0 blocked 1 "
        "[inserted state verified] [cleanup calls disabled] (inserted IR)"
    );
    assert(
        blocked_gate[1] ==
        "computed DynamicArray cleanup call emission gate detail owner items acquire "
        "items.computed_for.0.cleanup.acquire resume items.computed_for.0.cleanup.resume (inserted IR)"
    );

    auto ready_gate = driver::computed_cleanup_call_emission_gate_state_report(
        pipeline::ComputedCleanupCallEmissionGateState {
            .cleanup_owner_names = {"items"},
            .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
            .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
            .all_state_verified = true,
            .all_cleanup_calls_enabled = true,
            .all_ready = true,
            .gate_count = 1,
            .ready_count = 1,
            .blocked_count = 0,
        }
    );
    assert(ready_gate.size() == 2);
    assert(
        ready_gate[0] ==
        "computed DynamicArray cleanup call emission gate ready gates 1 ready 1 blocked 0 "
        "[inserted state verified] [cleanup calls enabled] (inserted IR)"
    );
    assert(
        ready_gate[1] ==
        "computed DynamicArray cleanup call emission gate detail owner items acquire "
        "items.computed_for.0.cleanup.acquire resume items.computed_for.0.cleanup.resume (inserted IR)"
    );
}

void assert_computed_cleanup_call_plan_and_render_reports() {
    auto blocked = pipeline::ComputedCleanupCallPlanRenderState {
        .cleanup_owner_names = {"items"},
        .cleanup_operation_names = {"items.computed_for.0.cleanup.resume.call"},
        .data_pointer_names = {"%items.computed_for.0.data"},
        .element_size_bytes = {"4"},
        .capacity_names = {"%items.computed_for.0.capacity"},
        .all_state_verified = true,
        .all_operands_proven = true,
        .all_cleanup_calls_enabled = false,
        .all_renderable = false,
        .plan_count = 1,
        .render_count = 1,
        .planned_count = 1,
        .renderable_count = 1,
    };
    auto blocked_plan = driver::computed_cleanup_call_plan_state_report(blocked);
    assert(blocked_plan.size() == 2);
    assert(
        blocked_plan[0] ==
        "computed DynamicArray cleanup call plan planned plans 1 planned 1 renderable 1 renders 1 "
        "[inserted state verified] [cleanup operands proven] [cleanup calls disabled] (inserted IR)"
    );
    assert(
        blocked_plan[1] ==
        "computed DynamicArray cleanup call plan detail owner items cleanup-operation "
        "items.computed_for.0.cleanup.resume.call data %items.computed_for.0.data element-size 4 capacity "
        "%items.computed_for.0.capacity (inserted IR)"
    );
    auto blocked_render = driver::computed_cleanup_call_render_state_report(blocked);
    assert(blocked_render.size() == 2);
    assert(
        blocked_render[0] ==
        "computed DynamicArray cleanup call render blocked renders 1 renderable 1 plans 1 "
        "[inserted state verified] [cleanup operands proven] [render blocked] (inserted IR)"
    );
    assert(
        blocked_render[1] ==
        "computed DynamicArray cleanup call render detail owner items cleanup-operation "
        "items.computed_for.0.cleanup.resume.call data %items.computed_for.0.data element-size 4 capacity "
        "%items.computed_for.0.capacity (inserted IR)"
    );

    auto rendered = blocked;
    rendered.all_cleanup_calls_enabled = true;
    rendered.all_renderable = true;
    auto rendered_plan = driver::computed_cleanup_call_plan_state_report(rendered);
    assert(rendered_plan.size() == 2);
    assert(
        rendered_plan[0] ==
        "computed DynamicArray cleanup call plan planned plans 1 planned 1 renderable 1 renders 1 "
        "[inserted state verified] [cleanup operands proven] [cleanup calls enabled] (inserted IR)"
    );
    auto rendered_call = driver::computed_cleanup_call_render_state_report(rendered);
    assert(rendered_call.size() == 2);
    assert(
        rendered_call[0] ==
        "computed DynamicArray cleanup call render rendered renders 1 renderable 1 plans 1 "
        "[inserted state verified] [cleanup operands proven] [renderable] (inserted IR)"
    );
}

void assert_computed_inserted_cleanup_call_reports() {
    auto absent = driver::computed_inserted_cleanup_call_state_report(
        pipeline::ComputedInsertedCleanupCallState {}
    );
    assert(absent.size() == 1);
    assert(absent.front() == smoke::computed_dynamic_array_inserted_cleanup_call_state_absent_report);

    auto inserted = driver::computed_inserted_cleanup_call_state_report(
        pipeline::ComputedInsertedCleanupCallState {
            .cleanup_owner_names = {"items"},
            .data_pointer_names = {"%items.computed_for.0.data"},
            .capacity_names = {"%items.computed_for.0.capacity"},
            .all_inserted = true,
            .call_count = 1,
            .structured_proof_count = 1,
            .ir_fallback_proof_count = 0,
        }
    );
    assert(inserted.size() == 2);
    assert(inserted[0] == smoke::computed_dynamic_array_inserted_cleanup_call_state_inserted_report);
    assert(inserted[1] == smoke::computed_dynamic_array_inserted_cleanup_call_state_detail_report);
}

void assert_computed_consumed_cleanup_descriptor_reports() {
    auto absent = driver::computed_consumed_cleanup_descriptor_state_report(
        pipeline::ComputedConsumedCleanupDescriptorState {}
    );
    assert(absent.size() == 1);
    assert(absent.front() == smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_absent_report);

    auto finalized = driver::computed_consumed_cleanup_descriptor_state_report(
        pipeline::ComputedConsumedCleanupDescriptorState {
            .cleanup_owner_names = {"items"},
            .descriptor_storage_names = {"%items.addr"},
            .all_finalized = true,
            .descriptor_count = 1,
            .structured_proof_count = 1,
            .ir_fallback_proof_count = 0,
        }
    );
    assert(finalized.size() == 2);
    assert(finalized[0] == smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_finalized_report);
    assert(finalized[1] == smoke::computed_dynamic_array_consumed_cleanup_descriptor_state_detail_report);
}

void assert_computed_cleanup_proof_summary_reports() {
    auto empty = driver::computed_cleanup_proof_summary_state_report(
        pipeline::ComputedCleanupProofSummaryState {}
    );
    assert(empty.size() == 1);
    assert(empty.front() == smoke::computed_dynamic_array_cleanup_proof_summary_empty_report);

    auto inserted = driver::computed_cleanup_proof_summary_state_report(
        pipeline::ComputedCleanupProofSummaryState {
            .cleanup_proof_model_count = 1,
            .verified_inserted_cleanup_pair_count = 1,
            .structured_inserted_cleanup_handoff_count = 2,
            .structured_inserted_cleanup_handoff_use_count = 2,
            .ir_inserted_cleanup_handoff_fallback_count = 0,
            .structured_cleanup_operand_count = 1,
            .structured_cleanup_operand_use_count = 1,
            .ir_cleanup_operand_fallback_count = 0,
            .structured_inserted_cleanup_call_count = 1,
            .ir_inserted_cleanup_call_fallback_count = 0,
            .structured_consumed_cleanup_descriptor_count = 1,
            .ir_consumed_cleanup_descriptor_fallback_count = 0,
        }
    );
    assert(inserted.size() == 1);
    assert(inserted.front() == smoke::computed_dynamic_array_cleanup_proof_summary_inserted_report);
}

void assert_computed_cleanup_unknown_detail_fallbacks() {
    auto readiness = driver::computed_cleanup_call_insertion_readiness_report(
        pipeline::ComputedCleanupCallInsertionGateState {
            .cleanup_owner_names = {"items"},
            .all_state_verified = true,
            .all_operands_proven = true,
            .all_cleanup_calls_authorized = false,
            .all_ready = false,
            .gate_count = 1,
            .ready_count = 0,
            .blocked_count = 1,
        }
    );
    assert(readiness.size() == 2);
    assert(
        readiness[1] ==
        "computed DynamicArray cleanup call insertion readiness detail owner items cleanup-operation <unknown> "
        "(metadata only)"
    );

    auto inserted = driver::computed_inserted_cleanup_call_state_report(
        pipeline::ComputedInsertedCleanupCallState {
            .cleanup_owner_names = {"items"},
            .all_inserted = true,
            .call_count = 1,
            .structured_proof_count = 1,
            .ir_fallback_proof_count = 0,
        }
    );
    assert(inserted.size() == 2);
    assert(
        inserted[1] ==
        "computed DynamicArray inserted cleanup call detail owner items data <unknown> capacity <unknown> "
        "(inserted IR)"
    );

    auto consumed = driver::computed_consumed_cleanup_descriptor_state_report(
        pipeline::ComputedConsumedCleanupDescriptorState {
            .cleanup_owner_names = {"items"},
            .all_finalized = true,
            .descriptor_count = 1,
            .structured_proof_count = 1,
            .ir_fallback_proof_count = 0,
        }
    );
    assert(consumed.size() == 2);
    assert(
        consumed[1] ==
        "computed DynamicArray consumed cleanup descriptor detail owner items descriptor <unknown> (inserted IR)"
    );

    auto handoff = driver::computed_inserted_cleanup_handoff_state_report(
        pipeline::ComputedInsertedCleanupHandoffState {
            .cleanup_owner_names = {"items"},
            .from_metadata = true,
            .all_paired = true,
            .all_cleanup_calls_enabled = false,
            .transition_count = 1,
            .verification_count = 1,
            .paired_count = 1,
            .blocked_count = 0,
        }
    );
    assert(handoff.size() == 2);
    assert(handoff[1] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_unknown_detail_report);
}

void assert_aggregate_projection_access_plan_reports() {
    auto empty = driver::aggregate_projection_access_plan_state_report(
        pipeline::AggregateProjectionAccessPlanState {}
    );
    assert(empty.empty());

    auto report = driver::aggregate_projection_access_plan_state_report(
        pipeline::AggregateProjectionAccessPlanState {
            .function_symbol_names = {"main", "main", "method.Box.payload"},
            .intents = {
                orison::lowering::AggregateProjectionAccessIntent::value_read,
                orison::lowering::AggregateProjectionAccessIntent::explicit_transfer,
                orison::lowering::AggregateProjectionAccessIntent::value_read,
            },
            .statuses = {
                orison::lowering::AggregateProjectionAccessStatus::requires_explicit_boundary,
                orison::lowering::AggregateProjectionAccessStatus::allowed,
                orison::lowering::AggregateProjectionAccessStatus::allowed,
            },
            .binding_names = {"box.payload", "box.payload", "this.payload"},
            .source_type_names = {"Payload", "Payload", "Payload"},
            .diagnostics = {
                "aggregate path read of owned projection requires an explicit ownership transfer",
                "",
                "",
            },
            .receiver_projections = {false, false, true},
            .access_plans_available = true,
            .plan_count = 3,
            .allowed_count = 2,
            .blocked_count = 1,
            .receiver_projection_count = 1,
        }
    );
    assert(report.size() == 3);
    assert(
        report[0] ==
        "function main aggregate projection access intent value_read status requires_explicit_boundary "
        "binding box.payload source Payload receiver false diagnostic aggregate path read of owned projection "
        "requires an explicit ownership transfer"
    );
    assert(
        report[1] ==
        "function main aggregate projection access intent explicit_transfer status allowed binding box.payload "
        "source Payload receiver false"
    );
    assert(
        report[2] ==
        "function method.Box.payload aggregate projection access intent value_read status allowed binding "
        "this.payload source Payload receiver true"
    );

    auto unknown_detail = driver::aggregate_projection_access_plan_state_report(
        pipeline::AggregateProjectionAccessPlanState {
            .function_symbol_names = {"main"},
            .access_plans_available = true,
            .plan_count = 1,
        }
    );
    assert(unknown_detail.size() == 1);
    assert(
        unknown_detail[0] ==
        "function main aggregate projection access intent <unknown> status <unknown> binding <unknown> "
        "source <unknown> receiver <unknown>"
    );
}

}  // namespace

auto main() -> int {
    assert_computed_cleanup_capability_reports();
    assert_computed_cleanup_readiness_reports();
    assert_computed_inserted_cleanup_handoff_reports();
    assert_computed_cleanup_verification_and_emission_gate_reports();
    assert_computed_cleanup_call_plan_and_render_reports();
    assert_computed_inserted_cleanup_call_reports();
    assert_computed_consumed_cleanup_descriptor_reports();
    assert_computed_cleanup_proof_summary_reports();
    assert_computed_cleanup_unknown_detail_fallbacks();
    assert_aggregate_projection_access_plan_reports();
    return 0;
}
