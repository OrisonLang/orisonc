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
    auto site_report = orison::semantics::format_planned_drop_site_report({site});
    assert(site_report.size() == 1);
    assert(site_report.front() == "drop site __orison_drop.Payload for Payload owner payload at line 12");
    assert(orison::semantics::format_planned_drop_site_report({}).empty());
    assert(orison::semantics::drop_abi_symbol_name("Pair<Payload>") == "__orison_drop.Pair_Payload_");

    auto unproven = orison::semantics::DropImplementation {
        .source_type_name = "Payload",
        .abi_symbol_name = "__orison_drop.Payload",
        .declaration_line = 7,
    };
    assert(
        orison::semantics::format_drop_implementation(unproven) ==
        "drop implementation __orison_drop.Payload for Payload declared at line 7 origin source-derived "
        "non-finite safe-boundary (unproven)"
    );
    auto missing = orison::semantics::resolve_drop_implementation(site, {unproven});
    assert(!missing.resolved);
    assert(
        orison::semantics::format_drop_implementation_resolution(missing) ==
        "missing drop site __orison_drop.Payload for Payload owner payload at line 12"
    );
    auto missing_report = orison::semantics::format_drop_implementation_resolution_report({site}, {unproven});
    assert(missing_report.size() == 1);
    assert(missing_report.front() == "missing drop site __orison_drop.Payload for Payload owner payload at line 12");
    assert(orison::semantics::format_drop_implementation_resolution_report({}, {}).empty());

    auto proven = unproven;
    proven.proven = true;
    proven.body.finite = true;
    assert(
        orison::semantics::format_drop_implementation(proven) ==
        "drop implementation __orison_drop.Payload for Payload declared at line 7 origin source-derived finite "
        "safe-boundary (proven)"
    );
    auto resolved = orison::semantics::resolve_drop_implementation(site, {proven});
    assert(resolved.resolved);
    assert(
        orison::semantics::format_drop_implementation_resolution(resolved) ==
        "resolved drop site __orison_drop.Payload for Payload owner payload at line 12"
    );
    auto resolved_report = orison::semantics::format_drop_implementation_resolution_report({site}, {proven});
    assert(resolved_report.size() == 1);
    assert(resolved_report.front() == "resolved drop site __orison_drop.Payload for Payload owner payload at line 12");

    auto source_derived = orison::semantics::source_derived_drop_implementation(
        "Payload",
        9,
        orison::semantics::DropImplementationBodySummary {
            .finite = true,
            .unsafe_boundary_required = true,
            .referenced_functions = {"payload_release", "audit_drop"},
        }
    );
    assert(source_derived.proven);
    assert(source_derived.abi_symbol_name == "__orison_drop.Payload");
    assert(source_derived.origin == orison::semantics::DropImplementationOrigin::source_derived);
    assert(
        orison::semantics::format_drop_implementation(source_derived) ==
        "drop implementation __orison_drop.Payload for Payload declared at line 9 origin source-derived finite "
        "unsafe-boundary references payload_release audit_drop (proven)"
    );
    auto resolved_source_derived =
        orison::semantics::resolve_drop_implementation(site, {source_derived});
    assert(resolved_source_derived.resolved);

    auto non_finite_source_derived = orison::semantics::source_derived_drop_implementation(
        "Payload",
        10,
        orison::semantics::DropImplementationBodySummary {}
    );
    assert(!non_finite_source_derived.proven);
    assert(
        orison::semantics::format_drop_implementation(non_finite_source_derived) ==
        "drop implementation __orison_drop.Payload for Payload declared at line 10 origin source-derived non-finite "
        "safe-boundary (unproven)"
    );
    assert(!orison::semantics::resolve_drop_implementation(site, {non_finite_source_derived}).resolved);

    auto test_fixture = source_derived;
    test_fixture.origin = orison::semantics::DropImplementationOrigin::test_fixture;
    assert(
        orison::semantics::format_drop_implementation(test_fixture) ==
        "drop implementation __orison_drop.Payload for Payload declared at line 9 origin test-fixture finite "
        "unsafe-boundary references payload_release audit_drop (proven)"
    );

    return 0;
}
