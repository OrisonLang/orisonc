#include "orison/lowering/concurrency_plan.hpp"

#include <cassert>
#include <string>

namespace {

auto payload_action() -> orison::lowering::PlannedDropAction {
    return orison::lowering::PlannedDropAction {
        .capture_name = "payload",
        .source_type_name = "Payload",
        .symbol_name = "__orison_drop.Payload",
        .field_index = 0,
        .discovery_line = 12,
    };
}

auto other_action() -> orison::lowering::PlannedDropAction {
    return orison::lowering::PlannedDropAction {
        .capture_name = "other",
        .source_type_name = "OtherPayload",
        .symbol_name = "__orison_drop.OtherPayload",
        .field_index = 1,
        .discovery_line = 20,
    };
}

auto cleanup_plan(int line = 12) -> orison::lowering::ConcurrencyDropCleanupPlan {
    return orison::lowering::ConcurrencyDropCleanupPlan {
        .cleanup_symbol_name = "__orison_thread_cleanup.launch." + std::to_string(line) + ".0",
    };
}

}  // namespace

auto main() -> int {
    auto action = payload_action();
    auto blocked_report = orison::lowering::format_drop_cleanup_authorization_report(
        cleanup_plan(),
        orison::lowering::DropCleanupAuthorizationReport {
            .semantic_lowering_blockers = {action},
            .semantic_unresolved_blockers = {action},
            .missing_declarations = {action},
        }
    );
    assert(blocked_report.size() == 4);
    assert(
        blocked_report[0] ==
        "drop cleanup authorization __orison_thread_cleanup.launch.12.0 blocked "
        "semantic blockers 1 missing declarations 1"
    );
    assert(
        blocked_report[1] ==
        "semantic drop lowering blocked __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );
    assert(
        blocked_report[2] ==
        "semantic drop unresolved __orison_drop.Payload for Payload capture payload field 0 discovered at line 12"
    );
    assert(
        blocked_report[3] ==
        "missing drop declaration __orison_drop.Payload for Payload capture payload field 0 discovered at line 12"
    );

    auto source_gated_report = orison::lowering::format_drop_cleanup_authorization_report(
        cleanup_plan(),
        orison::lowering::DropCleanupAuthorizationReport {
            .semantic_lowering_blockers = {action},
            .source_drop_lowering_blockers = {action},
        }
    );
    assert(source_gated_report.size() == 3);
    assert(
        source_gated_report[2] ==
        "source drop lowering not accepted __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );

    auto authorized_report = orison::lowering::format_drop_cleanup_authorization_report(
        cleanup_plan(),
        orison::lowering::DropCleanupAuthorizationReport {
            .authorized = true,
        }
    );
    assert(authorized_report.size() == 1);
    assert(authorized_report.front() == "drop cleanup authorization __orison_thread_cleanup.launch.12.0 authorized");

    auto other = other_action();
    auto multi_report = orison::lowering::format_drop_cleanup_authorization_report(
        cleanup_plan(20),
        orison::lowering::DropCleanupAuthorizationReport {
            .semantic_lowering_blockers = {action, other},
            .semantic_unresolved_blockers = {action, other},
            .missing_declarations = {action, other},
        }
    );
    assert(multi_report.size() == 7);
    assert(
        multi_report[0] ==
        "drop cleanup authorization __orison_thread_cleanup.launch.20.0 blocked "
        "semantic blockers 2 missing declarations 2"
    );
    assert(
        multi_report[2] ==
        "semantic drop lowering blocked __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(
        multi_report[4] ==
        "semantic drop unresolved __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(
        multi_report[6] ==
        "missing drop declaration __orison_drop.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );

    return 0;
}
