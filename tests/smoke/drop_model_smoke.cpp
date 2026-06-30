#include "orison/semantics/drop_model.hpp"

#include <cassert>

int main() {
    auto site = orison::semantics::PlannedDropSite {
        .source_type_name = "Payload",
        .abi_symbol_name = orison::semantics::drop_abi_symbol_name("Payload"),
        .owner_name = "payload",
        .site_line = 12,
    };
    assert(site.abi_symbol_name == "__orison_drop.Payload");
    assert(
        orison::semantics::format_planned_drop_site(site) ==
        "drop site __orison_drop.Payload for Payload owner payload at line 12"
    );
    assert(orison::semantics::drop_abi_symbol_name("Pair<Payload>") == "__orison_drop.Pair_Payload_");

    auto unproven = orison::semantics::DropImplementation {
        .source_type_name = "Payload",
        .abi_symbol_name = "__orison_drop.Payload",
        .declaration_line = 7,
    };
    assert(
        orison::semantics::format_drop_implementation(unproven) ==
        "drop implementation __orison_drop.Payload for Payload declared at line 7 (unproven)"
    );
    auto missing = orison::semantics::resolve_drop_implementation(site, {unproven});
    assert(!missing.resolved);
    assert(
        orison::semantics::format_drop_implementation_resolution(missing) ==
        "missing drop site __orison_drop.Payload for Payload owner payload at line 12"
    );

    auto proven = unproven;
    proven.proven = true;
    assert(
        orison::semantics::format_drop_implementation(proven) ==
        "drop implementation __orison_drop.Payload for Payload declared at line 7 (proven)"
    );
    auto resolved = orison::semantics::resolve_drop_implementation(site, {proven});
    assert(resolved.resolved);
    assert(
        orison::semantics::format_drop_implementation_resolution(resolved) ==
        "resolved drop site __orison_drop.Payload for Payload owner payload at line 12"
    );

    return 0;
}
