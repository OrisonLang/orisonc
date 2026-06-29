#include "orison/lowering/drop_metadata.hpp"

#include <cassert>
#include <vector>

int main() {
    using orison::lowering::PlannedDropDeclaration;
    using orison::lowering::add_planned_drop_declaration;
    using orison::lowering::format_planned_drop_declaration;
    using orison::lowering::format_planned_drop_report;
    using orison::lowering::planned_drop_declaration_for_action;

    auto metadata_only = PlannedDropDeclaration {
        .symbol_name = "__orison_drop.Payload",
        .source_type_name = "Payload",
        .discovery_line = 12,
    };
    assert(
        format_planned_drop_declaration(metadata_only) ==
        "planned drop __orison_drop.Payload for Payload discovered at line 12 (metadata only)"
    );

    auto enabled = metadata_only;
    enabled.emit_declaration = true;
    assert(
        format_planned_drop_declaration(enabled) ==
        "planned drop __orison_drop.Payload for Payload discovered at line 12"
    );

    assert(
        format_planned_drop_declaration(PlannedDropDeclaration {
            .symbol_name = "__orison_drop.Unknown",
        }) == "planned drop __orison_drop.Unknown (metadata only)"
    );

    auto planned_drops = std::vector<PlannedDropDeclaration> {};
    assert(add_planned_drop_declaration(planned_drops, metadata_only));
    assert(!add_planned_drop_declaration(
        planned_drops,
        PlannedDropDeclaration {
            .symbol_name = "__orison_drop.Payload",
            .source_type_name = "Payload",
            .discovery_line = 99,
        }
    ));
    assert(planned_drops.size() == 1);
    assert(planned_drops.front().discovery_line == 12);

    assert(add_planned_drop_declaration(
        planned_drops,
        PlannedDropDeclaration {
            .symbol_name = "__orison_drop.OtherPayload",
            .source_type_name = "OtherPayload",
            .discovery_line = 20,
        }
    ));
    assert(planned_drops.size() == 2);
    assert(planned_drops[1].symbol_name == "__orison_drop.OtherPayload");

    auto report = format_planned_drop_report(planned_drops);
    assert(report.size() == 2);
    assert(
        report[0] ==
        "planned drop __orison_drop.Payload for Payload discovered at line 12 (metadata only)"
    );
    assert(
        report[1] ==
        "planned drop __orison_drop.OtherPayload for OtherPayload discovered at line 20 (metadata only)"
    );

    auto empty_report = format_planned_drop_report({});
    assert(empty_report.empty());

    auto declaration_from_action = planned_drop_declaration_for_action(orison::lowering::PlannedDropAction {
        .capture_name = "payload",
        .source_type_name = "Payload",
        .symbol_name = "__orison_drop.Payload",
        .field_index = 3,
        .discovery_line = 42,
    });
    assert(declaration_from_action.symbol_name == "__orison_drop.Payload");
    assert(declaration_from_action.source_type_name == "Payload");
    assert(declaration_from_action.discovery_line == 42);
    assert(!declaration_from_action.emit_declaration);
    return 0;
}
