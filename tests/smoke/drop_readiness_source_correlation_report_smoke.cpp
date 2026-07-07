#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include <cassert>
#include <string>
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

auto payload_authorization(
    std::string owner_name,
    bool semantic_resolved,
    bool lowering_enabled = false
) -> orison::semantics::DropLoweringAuthorization {
    return orison::semantics::DropLoweringAuthorization {
        .site = orison::semantics::PlannedDropSite {
            .source_type_name = "Payload",
            .abi_symbol_name = "__orison_drop.Payload",
            .owner_name = std::move(owner_name),
            .site_line = 11,
        },
        .semantic_resolved = semantic_resolved,
        .source_drop_lowering_enabled = lowering_enabled,
        .authorized = semantic_resolved && lowering_enabled,
    };
}

}  // namespace

auto main() -> int {
    auto empty_report = orison::pipeline::format_drop_readiness_source_correlation_report({});
    assert(empty_report.size() == 1);
    assert(empty_report.front() == "drop readiness source correlations actions 0 semantic sites 0");

    auto action = payload_action();
    auto unresolved_snapshot = orison::lowering::DropReadinessSnapshot {
        .semantic_authorizations = {payload_authorization("payload", false)},
        .cleanup_authorizations = {
            orison::lowering::DropCleanupReadiness {
                .cleanup_symbol_name = "__orison_thread_cleanup.launch.12.0",
                .authorization = orison::lowering::DropCleanupAuthorizationReport {
                    .semantic_lowering_blockers = {action},
                    .semantic_unresolved_blockers = {action},
                    .missing_declarations = {action},
                },
            },
        },
    };
    auto unresolved_report =
        orison::pipeline::format_drop_readiness_source_correlation_report(unresolved_snapshot);
    assert(unresolved_report.size() == 2);
    assert(unresolved_report[0] == "drop readiness source correlations actions 1 semantic sites 1");
    assert(
        unresolved_report[1] ==
        "drop readiness source correlation __orison_thread_cleanup.launch.12.0 __orison_drop.Payload for Payload "
        "capture payload field 0 action line 12 semantic owner payload site line 11 semantic unresolved "
        "source lowering not accepted declaration missing"
    );

    auto resolved_snapshot = unresolved_snapshot;
    resolved_snapshot.semantic_authorizations = {payload_authorization("payload", true)};
    resolved_snapshot.cleanup_authorizations.front().authorization.semantic_unresolved_blockers.clear();
    resolved_snapshot.cleanup_authorizations.front().authorization.source_drop_lowering_blockers = {action};
    auto resolved_report =
        orison::pipeline::format_drop_readiness_source_correlation_report(resolved_snapshot);
    assert(resolved_report.size() == 2);
    assert(
        resolved_report[1] ==
        "drop readiness source correlation __orison_thread_cleanup.launch.12.0 __orison_drop.Payload for Payload "
        "capture payload field 0 action line 12 semantic owner payload site line 11 semantic resolved "
        "source lowering not accepted declaration missing"
    );

    auto fallback_snapshot = resolved_snapshot;
    fallback_snapshot.semantic_authorizations = {payload_authorization("other_owner", true, true)};
    fallback_snapshot.emitted_declarations = {
        orison::lowering::PlannedDropDeclaration {
            .symbol_name = "__orison_drop.Payload",
            .source_type_name = "Payload",
            .discovery_line = 12,
            .emit_declaration = true,
        },
    };
    auto fallback_report =
        orison::pipeline::format_drop_readiness_source_correlation_report(fallback_snapshot);
    assert(fallback_report.size() == 2);
    assert(
        fallback_report[1] ==
        "drop readiness source correlation __orison_thread_cleanup.launch.12.0 __orison_drop.Payload for Payload "
        "capture payload field 0 action line 12 semantic owner other_owner site line 11 semantic resolved "
        "source lowering accepted declaration emitted"
    );

    auto duplicate_snapshot = unresolved_snapshot;
    duplicate_snapshot.cleanup_authorizations.front().authorization.semantic_lowering_blockers.push_back(action);
    duplicate_snapshot.cleanup_authorizations.front().authorization.missing_declarations.push_back(action);
    auto duplicate_report =
        orison::pipeline::format_drop_readiness_source_correlation_report(duplicate_snapshot);
    assert(duplicate_report.size() == 2);
    assert(duplicate_report[0] == "drop readiness source correlations actions 1 semantic sites 1");

    return 0;
}
