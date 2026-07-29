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
            .all_state_verified = true,
            .all_operands_proven = true,
            .all_cleanup_calls_authorized = false,
            .all_ready = false,
            .gate_count = 1,
            .ready_count = 0,
            .blocked_count = 1,
        }
    );
    assert(blocked.size() == 2);
    assert(blocked[0] == smoke::computed_dynamic_array_cleanup_call_insertion_readiness_blocked_report);
    assert(blocked[1] == smoke::computed_dynamic_array_cleanup_call_insertion_readiness_detail_report);

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

    auto paired_disabled = driver::computed_inserted_cleanup_handoff_state_report(
        pipeline::ComputedInsertedCleanupHandoffState {
            .cleanup_owner_names = {"items"},
            .acquire_operation_names = {"items.computed_for.0.cleanup.acquire"},
            .resume_operation_names = {"items.computed_for.0.cleanup.resume"},
            .from_metadata = true,
            .all_paired = true,
            .all_cleanup_calls_enabled = false,
            .transition_count = 1,
            .verification_count = 1,
            .paired_count = 1,
            .blocked_count = 0,
        }
    );
    assert(paired_disabled.size() == 2);
    assert(paired_disabled[0] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_paired_disabled_report);
    assert(paired_disabled[1] == smoke::computed_dynamic_array_inserted_cleanup_handoff_state_detail_report);

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

}  // namespace

auto main() -> int {
    assert_computed_cleanup_capability_reports();
    assert_computed_cleanup_readiness_reports();
    assert_computed_inserted_cleanup_handoff_reports();
    assert_computed_inserted_cleanup_call_reports();
    assert_computed_consumed_cleanup_descriptor_reports();
    assert_computed_cleanup_proof_summary_reports();
    assert_computed_cleanup_unknown_detail_fallbacks();
    return 0;
}
