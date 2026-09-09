#include "orison/lowering/concurrency_plan.hpp"

#include <cassert>
#include <string>
#include <utility>

namespace {

auto payload_action() -> orison::lowering::OwnedCleanupAction {
    return orison::lowering::OwnedCleanupAction {
        .capture_name = "payload",
        .source_type_name = "Payload",
        .symbol_name = "__orison_owned_cleanup.Payload",
        .field_index = 0,
        .discovery_line = 12,
    };
}

auto other_action() -> orison::lowering::OwnedCleanupAction {
    return orison::lowering::OwnedCleanupAction {
        .capture_name = "other",
        .source_type_name = "OtherPayload",
        .symbol_name = "__orison_owned_cleanup.OtherPayload",
        .field_index = 1,
        .discovery_line = 20,
    };
}

auto cleanup_readiness(
    orison::lowering::OwnedCleanupAuthorizationReport authorization,
    int line = 12
) -> orison::lowering::OwnedCleanupReadiness {
    return orison::lowering::OwnedCleanupReadiness {
        .cleanup_symbol_name = "__orison_thread_cleanup.launch." + std::to_string(line) + ".0",
        .authorization = std::move(authorization),
    };
}

}  // namespace

auto main() -> int {
    auto empty_report = orison::lowering::format_owned_cleanup_readiness_relation_report({});
    assert(empty_report.empty());

    auto action = payload_action();
    auto blocked_snapshot = orison::lowering::OwnedCleanupReadinessSnapshot {
        .cleanup_authorizations = {
            cleanup_readiness(orison::lowering::OwnedCleanupAuthorizationReport {
                .semantic_lowering_blockers = {action},
                .missing_declarations = {action},
            }),
        },
    };
    auto blocked_report = orison::lowering::format_owned_cleanup_readiness_relation_report(blocked_snapshot);
    assert(blocked_report.size() == 3);
    assert(
        blocked_report[0] ==
        "drop readiness relation __orison_thread_cleanup.launch.12.0 blocked "
        "semantic blockers 1 emitted declarations 0 missing declarations 1"
    );
    assert(
        blocked_report[1] ==
        "drop readiness relation semantic blocker __orison_owned_cleanup.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );
    assert(
        blocked_report[2] ==
        "drop readiness relation missing declaration __orison_owned_cleanup.Payload for Payload capture payload field 0 "
        "discovered at line 12"
    );

    auto emitted_snapshot = orison::lowering::OwnedCleanupReadinessSnapshot {
        .emitted_declarations = {
            orison::lowering::OwnedCleanupDeclaration {
                .symbol_name = "__orison_owned_cleanup.Payload",
                .source_type_name = "Payload",
                .discovery_line = 12,
                .emit_declaration = true,
            },
        },
        .cleanup_authorizations = {
            cleanup_readiness(orison::lowering::OwnedCleanupAuthorizationReport {
                .authorized = true,
            }),
        },
    };
    auto emitted_report = orison::lowering::format_owned_cleanup_readiness_relation_report(emitted_snapshot);
    assert(emitted_report.size() == 1);
    assert(
        emitted_report[0] ==
        "drop readiness relation __orison_thread_cleanup.launch.12.0 authorized "
        "semantic blockers 0 emitted declarations 1 missing declarations 0"
    );

    auto other = other_action();
    auto multi_snapshot = orison::lowering::OwnedCleanupReadinessSnapshot {
        .cleanup_authorizations = {
            cleanup_readiness(
                orison::lowering::OwnedCleanupAuthorizationReport {
                    .semantic_lowering_blockers = {action, other},
                    .missing_declarations = {action, other},
                },
                20
            ),
        },
    };
    auto multi_report = orison::lowering::format_owned_cleanup_readiness_relation_report(multi_snapshot);
    assert(multi_report.size() == 5);
    assert(
        multi_report[0] ==
        "drop readiness relation __orison_thread_cleanup.launch.20.0 blocked "
        "semantic blockers 2 emitted declarations 0 missing declarations 2"
    );
    assert(
        multi_report[2] ==
        "drop readiness relation semantic blocker __orison_owned_cleanup.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );
    assert(
        multi_report[4] ==
        "drop readiness relation missing declaration __orison_owned_cleanup.OtherPayload for OtherPayload capture other field 1 "
        "discovered at line 20"
    );

    return 0;
}
