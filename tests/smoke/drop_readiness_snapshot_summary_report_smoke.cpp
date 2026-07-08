#include "orison/lowering/concurrency_plan.hpp"

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

auto drop_authorization(
    std::string source_type_name,
    std::string abi_symbol_name,
    bool authorized
) -> orison::semantics::DropLoweringAuthorization {
    return orison::semantics::DropLoweringAuthorization {
        .site = orison::semantics::PlannedDropSite {
            .source_type_name = std::move(source_type_name),
            .abi_symbol_name = std::move(abi_symbol_name),
            .owner_name = "payload",
            .site_line = 12,
        },
        .semantic_resolved = authorized,
        .source_drop_lowering_enabled = authorized,
        .authorized = authorized,
    };
}

auto cleanup_readiness(
    orison::lowering::DropCleanupAuthorizationReport authorization,
    int line = 12
) -> orison::lowering::DropCleanupReadiness {
    return orison::lowering::DropCleanupReadiness {
        .cleanup_symbol_name = "__orison_thread_cleanup.launch." + std::to_string(line) + ".0",
        .authorization = std::move(authorization),
    };
}

}  // namespace

auto main() -> int {
    auto empty_snapshot = orison::lowering::DropReadinessSnapshot {};
    auto empty_snapshot_report = orison::lowering::format_drop_readiness_snapshot_report(empty_snapshot);
    assert(empty_snapshot_report.size() == 1);
    assert(
        empty_snapshot_report.front() ==
        "drop readiness snapshot semantic authorizations 0 emitted declarations 0 cleanup authorizations 0"
    );
    auto empty_summary = orison::lowering::summarize_drop_readiness(empty_snapshot);
    assert(
        orison::lowering::format_drop_readiness_summary(empty_summary) ==
        "drop readiness summary semantic authorized 0 blocked 0 emitted declarations 0 cleanup authorized 0 blocked 0"
    );

    auto action = payload_action();
    auto blocked_snapshot = orison::lowering::DropReadinessSnapshot {
        .semantic_authorizations = {
            drop_authorization("Payload", "__orison_drop.Payload", false),
        },
        .cleanup_authorizations = {
            cleanup_readiness(orison::lowering::DropCleanupAuthorizationReport {
                .semantic_lowering_blockers = {action},
                .missing_declarations = {action},
            }),
        },
    };
    auto blocked_snapshot_report = orison::lowering::format_drop_readiness_snapshot_report(blocked_snapshot);
    assert(blocked_snapshot_report.size() == 3);
    assert(
        blocked_snapshot_report[0] ==
        "drop readiness snapshot semantic authorizations 1 emitted declarations 0 cleanup authorizations 1"
    );
    assert(blocked_snapshot_report[1] == "semantic readiness __orison_drop.Payload for Payload blocked");
    assert(
        blocked_snapshot_report[2] ==
        "cleanup readiness __orison_thread_cleanup.launch.12.0 blocked semantic blockers 1 missing declarations 1"
    );
    auto blocked_summary = orison::lowering::summarize_drop_readiness(blocked_snapshot);
    assert(
        orison::lowering::format_drop_readiness_summary(blocked_summary) ==
        "drop readiness summary semantic authorized 0 blocked 1 emitted declarations 0 cleanup authorized 0 blocked 1"
    );

    auto authorized_snapshot = orison::lowering::DropReadinessSnapshot {
        .semantic_authorizations = {
            drop_authorization("Payload", "__orison_drop.Payload", true),
        },
        .emitted_declarations = {
            orison::lowering::PlannedDropDeclaration {
                .symbol_name = "__orison_drop.Payload",
                .source_type_name = "Payload",
                .discovery_line = 12,
                .emit_declaration = true,
            },
        },
        .cleanup_authorizations = {
            cleanup_readiness(orison::lowering::DropCleanupAuthorizationReport {
                .authorized = true,
            }),
        },
    };
    auto authorized_snapshot_report = orison::lowering::format_drop_readiness_snapshot_report(authorized_snapshot);
    assert(authorized_snapshot_report.size() == 4);
    assert(
        authorized_snapshot_report[0] ==
        "drop readiness snapshot semantic authorizations 1 emitted declarations 1 cleanup authorizations 1"
    );
    assert(authorized_snapshot_report[1] == "semantic readiness __orison_drop.Payload for Payload authorized");
    assert(authorized_snapshot_report[2] == "emitted declaration readiness __orison_drop.Payload for Payload");
    assert(authorized_snapshot_report[3] == "cleanup readiness __orison_thread_cleanup.launch.12.0 authorized");
    auto authorized_summary = orison::lowering::summarize_drop_readiness(authorized_snapshot);
    assert(
        orison::lowering::format_drop_readiness_summary(authorized_summary) ==
        "drop readiness summary semantic authorized 1 blocked 0 emitted declarations 1 cleanup authorized 1 blocked 0"
    );

    auto mixed_snapshot = authorized_snapshot;
    mixed_snapshot.semantic_authorizations.push_back(
        drop_authorization("OtherPayload", "__orison_drop.OtherPayload", false)
    );
    mixed_snapshot.cleanup_authorizations.push_back(
        cleanup_readiness(
            orison::lowering::DropCleanupAuthorizationReport {
                .semantic_lowering_blockers = {action},
                .missing_declarations = {action},
            },
            20
        )
    );
    auto mixed_summary = orison::lowering::summarize_drop_readiness(mixed_snapshot);
    assert(
        orison::lowering::format_drop_readiness_summary(mixed_summary) ==
        "drop readiness summary semantic authorized 1 blocked 1 emitted declarations 1 cleanup authorized 1 blocked 1"
    );

    return 0;
}
