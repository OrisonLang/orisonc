#include "orison/semantics/drop_model.hpp"

#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <utility>

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
    auto unproven_diagnostic = orison::semantics::diagnose_drop_implementation(site, {unproven});
    assert(!unproven_diagnostic.resolved);
    assert(
        unproven_diagnostic.blocker_reason ==
        orison::semantics::DropImplementationBlockerReason::implementation_discovered_but_unproven
    );
    assert(
        orison::semantics::format_drop_implementation_diagnostic(unproven_diagnostic) ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner payload at line 12 blocked "
        "implementation discovered but unproven"
    );
    auto undiscovered_diagnostic = orison::semantics::diagnose_drop_implementation(site, {});
    assert(!undiscovered_diagnostic.resolved);
    assert(
        undiscovered_diagnostic.blocker_reason ==
        orison::semantics::DropImplementationBlockerReason::no_implementation_discovered
    );
    assert(
        orison::semantics::format_drop_implementation_diagnostic(undiscovered_diagnostic) ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner payload at line 12 blocked "
        "no implementation discovered"
    );
    auto diagnostic_report = orison::semantics::format_drop_implementation_diagnostic_report({site}, {unproven});
    assert(diagnostic_report.size() == 1);
    assert(
        diagnostic_report.front() ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner payload at line 12 blocked "
        "implementation discovered but unproven"
    );
    assert(orison::semantics::format_drop_implementation_diagnostic_report({}, {}).empty());

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
    auto resolved_diagnostic = orison::semantics::diagnose_drop_implementation(site, {unproven, proven});
    assert(resolved_diagnostic.resolved);
    assert(resolved_diagnostic.blocker_reason == orison::semantics::DropImplementationBlockerReason::none);
    assert(
        orison::semantics::format_drop_implementation_diagnostic(resolved_diagnostic) ==
        "drop diagnostic drop site __orison_drop.Payload for Payload owner payload at line 12 resolved"
    );
    assert(
        orison::semantics::format_drop_lowering_authorization(site, {unproven}) ==
        "drop lowering authorization drop site __orison_drop.Payload for Payload owner payload at line 12 "
        "semantic-unresolved lowering-blocked semantic drop unresolved"
    );
    auto unresolved_lowering_authorization = orison::semantics::authorize_drop_lowering(
        site,
        {unproven},
        orison::semantics::SourceDropLoweringGate::enabled
    );
    assert(!unresolved_lowering_authorization.semantic_resolved);
    assert(unresolved_lowering_authorization.source_drop_lowering_enabled);
    assert(!unresolved_lowering_authorization.authorized);
    auto disabled_lowering_authorization = orison::semantics::authorize_drop_lowering(site, {proven});
    assert(disabled_lowering_authorization.semantic_resolved);
    assert(!disabled_lowering_authorization.source_drop_lowering_enabled);
    assert(!disabled_lowering_authorization.authorized);
    auto enabled_lowering_authorization = orison::semantics::authorize_drop_lowering(
        site,
        {proven},
        orison::semantics::SourceDropLoweringGate::enabled
    );
    assert(enabled_lowering_authorization.semantic_resolved);
    assert(enabled_lowering_authorization.source_drop_lowering_enabled);
    assert(enabled_lowering_authorization.authorized);
    assert(
        orison::semantics::format_drop_lowering_authorization(site, {proven}) ==
        "drop lowering authorization drop site __orison_drop.Payload for Payload owner payload at line 12 "
        "semantic-resolved lowering-blocked source drop lowering not accepted"
    );
    auto lowering_authorization_report =
        orison::semantics::format_drop_lowering_authorization_report({site}, {proven});
    assert(lowering_authorization_report.size() == 1);
    assert(
        lowering_authorization_report.front() ==
        "drop lowering authorization drop site __orison_drop.Payload for Payload owner payload at line 12 "
        "semantic-resolved lowering-blocked source drop lowering not accepted"
    );
    assert(orison::semantics::format_drop_lowering_authorization_report({}, {}).empty());

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

    auto collected_implementations = orison::semantics::collect_source_derived_drop_implementations({
        orison::semantics::DropImplementationCandidate {},
        orison::semantics::DropImplementationCandidate {
            .source_type_name = "Payload",
            .declaration_line = 20,
            .body = orison::semantics::DropImplementationBodySummary {
                .finite = true,
                .referenced_functions = {"payload_release"},
            },
        },
        orison::semantics::DropImplementationCandidate {
            .source_type_name = "Payload",
            .declaration_line = 21,
            .body = orison::semantics::DropImplementationBodySummary {
                .finite = true,
                .referenced_functions = {"payload_release_duplicate"},
            },
        },
        orison::semantics::DropImplementationCandidate {
            .source_type_name = "Resource",
            .declaration_line = 22,
            .body = orison::semantics::DropImplementationBodySummary {},
        },
    });
    assert(collected_implementations.size() == 2);
    assert(collected_implementations[0].source_type_name == "Payload");
    assert(collected_implementations[0].abi_symbol_name == "__orison_drop.Payload");
    assert(collected_implementations[0].declaration_line == 20);
    assert(collected_implementations[0].proven);
    assert(collected_implementations[0].body.referenced_functions.size() == 1);
    assert(collected_implementations[0].body.referenced_functions.front() == "payload_release");
    assert(collected_implementations[1].source_type_name == "Resource");
    assert(collected_implementations[1].abi_symbol_name == "__orison_drop.Resource");
    assert(collected_implementations[1].declaration_line == 22);
    assert(!collected_implementations[1].proven);

    auto module = orison::syntax::ModuleSyntax {};
    auto drop_implementation = orison::syntax::ImplementationSyntax {};
    drop_implementation.interface_type.name = "Drop";
    drop_implementation.receiver_type.name = "Payload";
    drop_implementation.methods.push_back(orison::syntax::FunctionSyntax {
        .line = 30,
        .name = "drop",
    });
    module.implementations.push_back(std::move(drop_implementation));

    auto generic_drop_implementation = orison::syntax::ImplementationSyntax {};
    generic_drop_implementation.interface_type.name = "Drop";
    generic_drop_implementation.receiver_type.name = "Box";
    generic_drop_implementation.receiver_type.generic_arguments.push_back(orison::syntax::TypeSyntax {
        .name = "Payload",
    });
    generic_drop_implementation.methods.push_back(orison::syntax::FunctionSyntax {
        .line = 31,
        .name = "drop",
    });
    module.implementations.push_back(std::move(generic_drop_implementation));

    auto unrelated_implementation = orison::syntax::ImplementationSyntax {};
    unrelated_implementation.interface_type.name = "Display";
    unrelated_implementation.receiver_type.name = "Payload";
    unrelated_implementation.methods.push_back(orison::syntax::FunctionSyntax {
        .line = 32,
        .name = "drop",
    });
    module.implementations.push_back(std::move(unrelated_implementation));

    auto incomplete_drop_implementation = orison::syntax::ImplementationSyntax {};
    incomplete_drop_implementation.interface_type.name = "Drop";
    incomplete_drop_implementation.receiver_type.name = "Resource";
    incomplete_drop_implementation.methods.push_back(orison::syntax::FunctionSyntax {
        .line = 33,
        .name = "release",
    });
    module.implementations.push_back(std::move(incomplete_drop_implementation));

    auto nonfinite_drop_implementation = orison::syntax::ImplementationSyntax {};
    nonfinite_drop_implementation.interface_type.name = "Drop";
    nonfinite_drop_implementation.receiver_type.name = "Resource";
    auto nonfinite_drop_method = orison::syntax::FunctionSyntax {
        .line = 34,
        .name = "drop",
    };
    nonfinite_drop_method.body_statements.push_back(orison::syntax::StatementSyntax {
        .kind = orison::syntax::StatementKind::let_binding,
        .line = 35,
        .name = "scratch",
    });
    nonfinite_drop_implementation.methods.push_back(std::move(nonfinite_drop_method));
    module.implementations.push_back(std::move(nonfinite_drop_implementation));

    auto empty_body_summary =
        orison::semantics::prove_source_derived_drop_implementation_body(module.implementations[0]);
    assert(empty_body_summary.finite);
    assert(!empty_body_summary.unsafe_boundary_required);
    assert(empty_body_summary.referenced_functions.empty());

    auto naked_return_implementation = orison::syntax::ImplementationSyntax {};
    naked_return_implementation.interface_type.name = "Drop";
    naked_return_implementation.receiver_type.name = "Payload";
    auto naked_return_method = orison::syntax::FunctionSyntax {
        .line = 36,
        .name = "drop",
    };
    naked_return_method.body_statements.push_back(orison::syntax::StatementSyntax {
        .kind = orison::syntax::StatementKind::return_statement,
        .line = 37,
    });
    naked_return_implementation.methods.push_back(std::move(naked_return_method));
    auto naked_return_body_summary =
        orison::semantics::prove_source_derived_drop_implementation_body(naked_return_implementation);
    assert(naked_return_body_summary.finite);
    assert(!naked_return_body_summary.unsafe_boundary_required);
    assert(naked_return_body_summary.referenced_functions.empty());

    auto source_candidates = orison::semantics::collect_source_derived_drop_implementation_candidates(module);
    assert(source_candidates.size() == 3);
    assert(source_candidates[0].source_type_name == "Payload");
    assert(source_candidates[0].declaration_line == 30);
    assert(source_candidates[0].body.finite);
    assert(!source_candidates[0].body.unsafe_boundary_required);
    assert(source_candidates[0].body.referenced_functions.empty());
    assert(source_candidates[1].source_type_name == "Box<Payload>");
    assert(source_candidates[1].declaration_line == 31);
    assert(source_candidates[1].body.finite);
    assert(!source_candidates[1].body.unsafe_boundary_required);
    assert(source_candidates[1].body.referenced_functions.empty());
    assert(source_candidates[2].source_type_name == "Resource");
    assert(source_candidates[2].declaration_line == 34);
    assert(!source_candidates[2].body.finite);
    assert(!source_candidates[2].body.unsafe_boundary_required);
    assert(source_candidates[2].body.referenced_functions.empty());

    auto resource_site = orison::semantics::PlannedDropSite {
        .source_type_name = "Resource",
        .abi_symbol_name = orison::semantics::drop_abi_symbol_name("Resource"),
        .owner_name = "resource",
        .site_line = 13,
    };
    auto second_payload_site = site;
    second_payload_site.owner_name = "other_payload";
    second_payload_site.site_line = 14;
    auto second_resource_site = resource_site;
    second_resource_site.owner_name = "other_resource";
    second_resource_site.site_line = 15;
    auto summaries = orison::semantics::summarize_drop_implementation_resolutions(
        {site, resource_site, second_payload_site, second_resource_site},
        collected_implementations
    );
    assert(summaries.size() == 2);
    assert(summaries[0].source_type_name == "Payload");
    assert(summaries[0].abi_symbol_name == "__orison_drop.Payload");
    assert(summaries[0].resolved_sites == 2);
    assert(summaries[0].missing_sites == 0);
    assert(summaries[1].source_type_name == "Resource");
    assert(summaries[1].abi_symbol_name == "__orison_drop.Resource");
    assert(summaries[1].resolved_sites == 0);
    assert(summaries[1].missing_sites == 2);
    assert(
        orison::semantics::format_drop_implementation_resolution_summary(summaries[0]) ==
        "drop resolution summary __orison_drop.Payload for Payload resolved 2 missing 0"
    );
    auto summary_report = orison::semantics::format_drop_implementation_resolution_summary_report(summaries);
    assert(summary_report.size() == 2);
    assert(
        summary_report[0] ==
        "drop resolution summary __orison_drop.Payload for Payload resolved 2 missing 0"
    );
    assert(
        summary_report[1] ==
        "drop resolution summary __orison_drop.Resource for Resource resolved 0 missing 2"
    );
    assert(orison::semantics::summarize_drop_implementation_resolutions({}, {}).empty());
    assert(orison::semantics::format_drop_implementation_resolution_summary_report({}).empty());

    return 0;
}
