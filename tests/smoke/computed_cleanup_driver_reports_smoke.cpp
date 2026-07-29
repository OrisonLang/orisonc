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

}  // namespace

auto main() -> int {
    assert_computed_cleanup_capability_reports();
    assert_computed_cleanup_readiness_reports();
    assert_computed_inserted_cleanup_call_reports();
    assert_computed_consumed_cleanup_descriptor_reports();
    return 0;
}
