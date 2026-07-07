#include "orison/lowering/concurrency_plan.hpp"

#include <cassert>
#include <utility>

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

auto cleanup_readiness(
    orison::lowering::DropCleanupAuthorizationReport authorization
) -> orison::lowering::DropCleanupReadiness {
    return orison::lowering::DropCleanupReadiness {
        .cleanup_symbol_name = "__orison_thread_cleanup.launch.12.0",
        .authorization = std::move(authorization),
    };
}

}  // namespace

auto main() -> int {
    auto empty_summary = orison::lowering::summarize_drop_readiness_blockers({});
    assert(empty_summary.blocked_cleanups == 0);
    assert(empty_summary.semantic_lowering_blockers.empty());
    assert(empty_summary.semantic_unresolved_blockers.empty());
    assert(empty_summary.source_drop_lowering_blockers.empty());
    assert(empty_summary.missing_declarations.empty());
    auto empty_report = orison::lowering::format_drop_readiness_blocker_report(empty_summary);
    assert(empty_report.size() == 1);
    assert(
        empty_report.front() ==
        "drop readiness blockers cleanups 0 semantic blockers 0 semantic unresolved 0 "
        "source lowering blocked 0 missing declarations 0"
    );

    auto action = payload_action();
    auto unresolved_snapshot = orison::lowering::DropReadinessSnapshot {
        .cleanup_authorizations = {
            cleanup_readiness(orison::lowering::DropCleanupAuthorizationReport {
                .semantic_lowering_blockers = {action},
                .semantic_unresolved_blockers = {action},
                .missing_declarations = {action},
            }),
        },
    };
    auto unresolved_summary = orison::lowering::summarize_drop_readiness_blockers(unresolved_snapshot);
    assert(unresolved_summary.blocked_cleanups == 1);
    assert(unresolved_summary.semantic_lowering_blockers.size() == 1);
    assert(unresolved_summary.semantic_unresolved_blockers.size() == 1);
    assert(unresolved_summary.source_drop_lowering_blockers.empty());
    assert(unresolved_summary.missing_declarations.size() == 1);
    auto unresolved_report = orison::lowering::format_drop_readiness_blocker_report(unresolved_summary);
    assert(unresolved_report.size() == 4);
    assert(
        unresolved_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 1 "
        "source lowering blocked 0 missing declarations 1"
    );
    assert(
        unresolved_report[1] ==
        "drop readiness blocker semantic __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );
    assert(
        unresolved_report[2] ==
        "drop readiness blocker semantic unresolved __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );
    assert(
        unresolved_report[3] ==
        "drop readiness blocker missing declaration __orison_drop.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );

    auto source_gated_snapshot = orison::lowering::DropReadinessSnapshot {
        .cleanup_authorizations = {
            cleanup_readiness(orison::lowering::DropCleanupAuthorizationReport {
                .semantic_lowering_blockers = {action},
                .source_drop_lowering_blockers = {action},
                .missing_declarations = {action},
            }),
        },
    };
    auto source_gated_summary = orison::lowering::summarize_drop_readiness_blockers(source_gated_snapshot);
    assert(source_gated_summary.blocked_cleanups == 1);
    assert(source_gated_summary.semantic_lowering_blockers.size() == 1);
    assert(source_gated_summary.semantic_unresolved_blockers.empty());
    assert(source_gated_summary.source_drop_lowering_blockers.size() == 1);
    assert(source_gated_summary.missing_declarations.size() == 1);
    auto source_gated_report = orison::lowering::format_drop_readiness_blocker_report(source_gated_summary);
    assert(source_gated_report.size() == 4);
    assert(
        source_gated_report[0] ==
        "drop readiness blockers cleanups 1 semantic blockers 1 semantic unresolved 0 "
        "source lowering blocked 1 missing declarations 1"
    );
    assert(
        source_gated_report[2] ==
        "drop readiness blocker source lowering not accepted __orison_drop.Payload for Payload capture payload "
        "field 0 discovered at line 12"
    );

    auto authorized_snapshot = orison::lowering::DropReadinessSnapshot {
        .cleanup_authorizations = {
            cleanup_readiness(orison::lowering::DropCleanupAuthorizationReport {
                .authorized = true,
                .semantic_lowering_blockers = {action},
                .missing_declarations = {action},
            }),
        },
    };
    auto authorized_summary = orison::lowering::summarize_drop_readiness_blockers(authorized_snapshot);
    assert(authorized_summary.blocked_cleanups == 0);
    assert(authorized_summary.semantic_lowering_blockers.empty());
    assert(authorized_summary.missing_declarations.empty());

    return 0;
}
