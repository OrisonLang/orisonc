#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <cassert>
#include <vector>

int main() {
    using orison::lowering::OwnedCleanupDeclaration;
    using orison::lowering::add_owned_cleanup_declaration;
    using orison::lowering::declared_owned_cleanup_declarations_for_authorized_semantic_drops;
    using orison::lowering::declared_owned_cleanup_declarations_for_allowed_source_types;
    using orison::lowering::format_owned_cleanup_action;
    using orison::lowering::format_owned_cleanup_action_report;
    using orison::lowering::format_emitted_owned_cleanup_declaration_report;
    using orison::lowering::format_owned_cleanup_declaration;
    using orison::lowering::format_owned_cleanup_declaration_report;
    using orison::lowering::owned_cleanup_declaration_for_authorization;
    using orison::lowering::owned_cleanup_declaration_for_action;

    auto metadata_only = OwnedCleanupDeclaration {
        .symbol_name = "__orison_owned_cleanup.Payload",
        .source_type_name = "Payload",
        .discovery_line = 12,
    };
    assert(
        format_owned_cleanup_declaration(metadata_only) ==
        "planned drop __orison_owned_cleanup.Payload for Payload discovered at line 12 (metadata only)"
    );

    auto enabled = metadata_only;
    enabled.emit_declaration = true;
    assert(
        format_owned_cleanup_declaration(enabled) ==
        "planned drop __orison_owned_cleanup.Payload for Payload discovered at line 12"
    );

    assert(
        format_owned_cleanup_declaration(OwnedCleanupDeclaration {
            .symbol_name = "__orison_owned_cleanup.Unknown",
        }) == "planned drop __orison_owned_cleanup.Unknown (metadata only)"
    );

    auto planned_drops = std::vector<OwnedCleanupDeclaration> {};
    assert(add_owned_cleanup_declaration(planned_drops, metadata_only));
    assert(!add_owned_cleanup_declaration(
        planned_drops,
        OwnedCleanupDeclaration {
            .symbol_name = "__orison_owned_cleanup.Payload",
            .source_type_name = "Payload",
            .discovery_line = 99,
        }
    ));
    assert(planned_drops.size() == 1);
    assert(planned_drops.front().discovery_line == 12);
    assert(!planned_drops.front().emit_declaration);

    assert(!add_owned_cleanup_declaration(
        planned_drops,
        OwnedCleanupDeclaration {
            .symbol_name = "__orison_owned_cleanup.Payload",
            .source_type_name = "Payload",
            .discovery_line = 100,
            .emit_declaration = true,
        }
    ));
    assert(planned_drops.size() == 1);
    assert(planned_drops.front().discovery_line == 12);
    assert(planned_drops.front().emit_declaration);
    planned_drops.front().emit_declaration = false;

    assert(add_owned_cleanup_declaration(
        planned_drops,
        OwnedCleanupDeclaration {
            .symbol_name = "__orison_owned_cleanup.OtherPayload",
            .source_type_name = "OtherPayload",
            .discovery_line = 20,
        }
    ));
    assert(planned_drops.size() == 2);
    assert(planned_drops[1].symbol_name == "__orison_owned_cleanup.OtherPayload");

    auto report = format_owned_cleanup_declaration_report(planned_drops);
    assert(report.size() == 2);
    assert(
        report[0] ==
        "planned drop __orison_owned_cleanup.Payload for Payload discovered at line 12 (metadata only)"
    );
    assert(
        report[1] ==
        "planned drop __orison_owned_cleanup.OtherPayload for OtherPayload discovered at line 20 (metadata only)"
    );

    auto empty_report = format_owned_cleanup_declaration_report({});
    assert(empty_report.empty());
    auto emitted_report = format_emitted_owned_cleanup_declaration_report({
        metadata_only,
        enabled,
    });
    assert(emitted_report.size() == 1);
    assert(emitted_report.front() == "planned drop __orison_owned_cleanup.Payload for Payload discovered at line 12");
    assert(format_emitted_owned_cleanup_declaration_report({metadata_only}).empty());

    auto action = orison::lowering::OwnedCleanupAction {
        .capture_name = "payload",
        .source_type_name = "Payload",
        .symbol_name = "__orison_owned_cleanup.Payload",
        .field_index = 3,
        .discovery_line = 42,
    };
    auto declaration_from_action = owned_cleanup_declaration_for_action(action);
    assert(declaration_from_action.symbol_name == "__orison_owned_cleanup.Payload");
    assert(declaration_from_action.source_type_name == "Payload");
    assert(declaration_from_action.discovery_line == 42);
    assert(!declaration_from_action.emit_declaration);

    auto unresolved_authorization = orison::semantics::OwnedCleanupLoweringAuthorization {
        .site = orison::semantics::OwnedCleanupSite {
            .source_type_name = "Payload",
            .abi_symbol_name = "__orison_owned_cleanup.Payload",
            .owner_name = "payload",
            .site_line = 7,
        },
        .semantic_resolved = false,
        .source_owned_cleanup_lowering_enabled = true,
        .authorized = false,
    };
    auto disabled_authorization = unresolved_authorization;
    disabled_authorization.semantic_resolved = true;
    disabled_authorization.source_owned_cleanup_lowering_enabled = false;
    auto authorized_authorization = disabled_authorization;
    authorized_authorization.source_owned_cleanup_lowering_enabled = true;
    authorized_authorization.authorized = true;

    auto declaration_from_authorization = owned_cleanup_declaration_for_authorization(authorized_authorization);
    assert(declaration_from_authorization.symbol_name == "__orison_owned_cleanup.Payload");
    assert(declaration_from_authorization.source_type_name == "Payload");
    assert(declaration_from_authorization.discovery_line == 7);
    assert(declaration_from_authorization.emit_declaration);

    assert(declared_owned_cleanup_declarations_for_authorized_semantic_drops({
        unresolved_authorization,
        disabled_authorization,
    }).empty());

    auto semantic_declarations = declared_owned_cleanup_declarations_for_authorized_semantic_drops({
        unresolved_authorization,
        authorized_authorization,
        orison::semantics::OwnedCleanupLoweringAuthorization {
            .site = orison::semantics::OwnedCleanupSite {
                .source_type_name = "Payload",
                .abi_symbol_name = "__orison_owned_cleanup.Payload",
                .owner_name = "other",
                .site_line = 9,
            },
            .semantic_resolved = true,
            .source_owned_cleanup_lowering_enabled = true,
            .authorized = true,
        },
    });
    assert(semantic_declarations.size() == 1);
    assert(semantic_declarations.front().symbol_name == "__orison_owned_cleanup.Payload");
    assert(semantic_declarations.front().discovery_line == 7);
    assert(semantic_declarations.front().emit_declaration);

    assert(
        format_owned_cleanup_action(action) ==
        "planned drop action __orison_owned_cleanup.Payload for capture payload: Payload field 3 "
        "discovered at line 42 (metadata only)"
    );
    auto action_report = format_owned_cleanup_action_report({action});
    assert(action_report.size() == 1);
    assert(action_report.front() == format_owned_cleanup_action(action));
    assert(format_owned_cleanup_action_report({}).empty());

    auto declared_test_drops = declared_owned_cleanup_declarations_for_allowed_source_types(
        {
            action,
            orison::lowering::OwnedCleanupAction {
                .capture_name = "payload_again",
                .source_type_name = "Payload",
                .symbol_name = "__orison_owned_cleanup.Payload",
                .field_index = 4,
                .discovery_line = 50,
            },
            orison::lowering::OwnedCleanupAction {
                .capture_name = "other",
                .source_type_name = "OtherPayload",
                .symbol_name = "__orison_owned_cleanup.OtherPayload",
                .field_index = 5,
                .discovery_line = 51,
            },
        },
        {"Payload"}
    );
    assert(declared_test_drops.size() == 1);
    assert(declared_test_drops.front().symbol_name == "__orison_owned_cleanup.Payload");
    assert(declared_test_drops.front().source_type_name == "Payload");
    assert(declared_test_drops.front().discovery_line == 42);
    assert(declared_test_drops.front().emit_declaration);

    auto dynamic_array_action = orison::lowering::OwnedCleanupAction {
        .capture_name = "items",
        .source_type_name = "DynamicArray<UInt32>",
        .symbol_name = orison::semantics::owned_cleanup_abi_symbol_name("DynamicArray<UInt32>"),
        .field_index = 1,
        .discovery_line = 64,
    };
    auto dynamic_array_invariants = orison::lowering::dynamic_array_lowering_invariants();
    assert(dynamic_array_invariants.element_drop_walk_required);
    assert(!dynamic_array_invariants.lowered_signatures_enabled);
    assert(declared_owned_cleanup_declarations_for_allowed_source_types({dynamic_array_action}, {}).empty());
    assert(declared_owned_cleanup_declarations_for_authorized_semantic_drops({}).empty());
    return 0;
}
