#include "orison/semantics/drop_model.hpp"

#include "orison/syntax/module_parser.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>
#include <utility>

namespace orison::semantics {
namespace {

auto render_source_type_name(syntax::TypeSyntax const& type) -> std::string {
    auto rendered = type.name;
    if (type.generic_arguments.empty()) {
        return rendered;
    }
    rendered += "<";
    for (std::size_t index = 0; index < type.generic_arguments.size(); ++index) {
        if (index > 0) {
            rendered += ", ";
        }
        rendered += render_source_type_name(type.generic_arguments[index]);
    }
    rendered += ">";
    return rendered;
}

auto is_drop_interface(syntax::TypeSyntax const& type) -> bool {
    return type.name == "Drop" && type.generic_arguments.empty();
}

auto has_drop_method(syntax::ImplementationSyntax const& implementation) -> bool {
    return std::find_if(
        implementation.methods.begin(),
        implementation.methods.end(),
        [](syntax::FunctionSyntax const& method) {
            return method.name == "drop";
        }
    ) != implementation.methods.end();
}

auto drop_method_line(syntax::ImplementationSyntax const& implementation) -> std::size_t {
    auto method = std::find_if(
        implementation.methods.begin(),
        implementation.methods.end(),
        [](syntax::FunctionSyntax const& candidate) {
            return candidate.name == "drop";
        }
    );
    return method == implementation.methods.end() ? 0 : method->line;
}

auto is_empty_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.text.empty() && expression.arguments.empty() && expression.nested_statements.empty() &&
           !expression.left && !expression.right && !expression.alternate;
}

auto is_naked_return(syntax::StatementSyntax const& statement) -> bool {
    return statement.kind == syntax::StatementKind::return_statement && is_empty_expression(statement.expression);
}

}  // namespace

auto owned_cleanup_abi_symbol_name(std::string_view source_type_name) -> std::string {
    auto symbol = std::string {"__orison_owned_cleanup."};
    for (auto character : source_type_name) {
        auto const allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9');
        symbol.push_back(allowed ? character : '_');
    }
    return symbol;
}

auto owned_cleanup_implementation_origin_name(OwnedCleanupImplementationOrigin origin) -> std::string_view {
    switch (origin) {
    case OwnedCleanupImplementationOrigin::source_derived:
        return "source-derived";
    case OwnedCleanupImplementationOrigin::compiler_intrinsic:
        return "compiler-intrinsic";
    case OwnedCleanupImplementationOrigin::test_fixture:
        return "test-fixture";
    }
    return "unknown";
}

auto source_derived_owned_cleanup_implementation(
    std::string source_type_name,
    std::size_t declaration_line,
    OwnedCleanupImplementationBodySummary body
) -> OwnedCleanupImplementation {
    auto symbol_name = owned_cleanup_abi_symbol_name(source_type_name);
    auto proven = body.finite;
    return OwnedCleanupImplementation {
        .source_type_name = std::move(source_type_name),
        .abi_symbol_name = std::move(symbol_name),
        .declaration_line = declaration_line,
        .proven = proven,
        .origin = OwnedCleanupImplementationOrigin::source_derived,
        .body = std::move(body),
    };
}

auto compiler_intrinsic_owned_cleanup_implementation(
    std::string source_type_name,
    std::size_t declaration_line
) -> OwnedCleanupImplementation {
    auto symbol_name = owned_cleanup_abi_symbol_name(source_type_name);
    return OwnedCleanupImplementation {
        .source_type_name = std::move(source_type_name),
        .abi_symbol_name = std::move(symbol_name),
        .declaration_line = declaration_line,
        .proven = true,
        .origin = OwnedCleanupImplementationOrigin::compiler_intrinsic,
        .body = OwnedCleanupImplementationBodySummary {
            .finite = true,
        },
    };
}

auto collect_source_derived_owned_cleanup_implementations(
    std::vector<OwnedCleanupImplementationCandidate> const& candidates
) -> std::vector<OwnedCleanupImplementation> {
    auto implementations = std::vector<OwnedCleanupImplementation> {};
    for (auto const& candidate : candidates) {
        if (candidate.source_type_name.empty()) {
            continue;
        }
        auto symbol_name = owned_cleanup_abi_symbol_name(candidate.source_type_name);
        auto existing = std::find_if(
            implementations.begin(),
            implementations.end(),
            [&candidate, &symbol_name](OwnedCleanupImplementation const& implementation) {
                return implementation.source_type_name == candidate.source_type_name &&
                       implementation.abi_symbol_name == symbol_name;
            }
        );
        if (existing != implementations.end()) {
            continue;
        }
        implementations.push_back(source_derived_owned_cleanup_implementation(
            candidate.source_type_name,
            candidate.declaration_line,
            candidate.body
        ));
    }
    return implementations;
}

auto prove_source_derived_owned_cleanup_implementation_body(
    syntax::ImplementationSyntax const& implementation
) -> OwnedCleanupImplementationBodySummary {
    auto method = std::find_if(
        implementation.methods.begin(),
        implementation.methods.end(),
        [](syntax::FunctionSyntax const& candidate) {
            return candidate.name == "drop";
        }
    );
    if (method == implementation.methods.end()) {
        return OwnedCleanupImplementationBodySummary {};
    }
    if (method->body_statements.empty()) {
        return OwnedCleanupImplementationBodySummary {
            .finite = true,
        };
    }
    if (method->body_statements.size() == 1 && is_naked_return(method->body_statements.front())) {
        return OwnedCleanupImplementationBodySummary {
            .finite = true,
        };
    }
    return OwnedCleanupImplementationBodySummary {};
}

auto collect_source_derived_owned_cleanup_implementation_candidates(
    syntax::ModuleSyntax const& module
) -> std::vector<OwnedCleanupImplementationCandidate> {
    auto candidates = std::vector<OwnedCleanupImplementationCandidate> {};
    for (auto const& implementation : module.implementations) {
        if (!is_drop_interface(implementation.interface_type) || !has_drop_method(implementation)) {
            continue;
        }
        candidates.push_back(OwnedCleanupImplementationCandidate {
            .source_type_name = render_source_type_name(implementation.receiver_type),
            .declaration_line = drop_method_line(implementation),
            .body = prove_source_derived_owned_cleanup_implementation_body(implementation),
        });
    }
    return candidates;
}

auto collect_compiler_intrinsic_owned_cleanup_implementations(
    std::vector<OwnedCleanupSite> const& sites,
    syntax::ModuleSyntax const& module
) -> std::vector<OwnedCleanupImplementation> {
    auto implementations = std::vector<OwnedCleanupImplementation> {};
    for (auto const& site : sites) {
        if (site.source_type_name.empty()) {
            continue;
        }
        auto const generic_start = site.source_type_name.find('<');
        auto const base_name = generic_start == std::string::npos
                                   ? site.source_type_name
                                   : site.source_type_name.substr(0, generic_start);
        auto record = std::find_if(
            module.records.begin(),
            module.records.end(),
            [&](syntax::RecordSyntax const& candidate) {
                return candidate.name == base_name;
            }
        );
        if (record == module.records.end()) {
            continue;
        }
        auto const symbol_name = owned_cleanup_abi_symbol_name(site.source_type_name);
        auto existing = std::find_if(
            implementations.begin(),
            implementations.end(),
            [&](OwnedCleanupImplementation const& implementation) {
                return implementation.source_type_name == site.source_type_name &&
                       implementation.abi_symbol_name == symbol_name;
            }
        );
        if (existing != implementations.end()) {
            continue;
        }
        implementations.push_back(compiler_intrinsic_owned_cleanup_implementation(
            site.source_type_name,
            record->line
        ));
    }
    return implementations;
}

auto format_owned_cleanup_implementation(OwnedCleanupImplementation const& implementation) -> std::string {
    auto output = std::ostringstream {};
    output << "drop implementation " << implementation.abi_symbol_name;
    if (!implementation.source_type_name.empty()) {
        output << " for " << implementation.source_type_name;
    }
    if (implementation.declaration_line > 0) {
        output << " declared at line " << implementation.declaration_line;
    }
    output << " origin " << owned_cleanup_implementation_origin_name(implementation.origin);
    output << (implementation.body.finite ? " finite" : " non-finite");
    output << (implementation.body.unsafe_boundary_required ? " unsafe-boundary" : " safe-boundary");
    if (!implementation.body.referenced_functions.empty()) {
        output << " references";
        for (auto const& referenced_function : implementation.body.referenced_functions) {
            output << " " << referenced_function;
        }
    }
    output << (implementation.proven ? " (proven)" : " (unproven)");
    return output.str();
}

auto format_planned_drop_site(OwnedCleanupSite const& site) -> std::string {
    auto output = std::ostringstream {};
    output << "drop site " << site.abi_symbol_name;
    if (!site.source_type_name.empty()) {
        output << " for " << site.source_type_name;
    }
    if (!site.owner_name.empty()) {
        output << " owner " << site.owner_name;
    }
    if (site.site_line > 0) {
        output << " at line " << site.site_line;
    }
    return output.str();
}

auto format_planned_drop_site_report(std::vector<OwnedCleanupSite> const& sites) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(sites.size());
    for (auto const& site : sites) {
        report.push_back(format_planned_drop_site(site));
    }
    return report;
}

auto resolve_owned_cleanup_implementation(
    OwnedCleanupSite site,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> OwnedCleanupImplementationResolution {
    for (auto const& implementation : implementations) {
        if (implementation.source_type_name == site.source_type_name &&
            implementation.abi_symbol_name == site.abi_symbol_name &&
            implementation.proven) {
            return OwnedCleanupImplementationResolution {
                .site = std::move(site),
                .resolved = true,
            };
        }
    }
    return OwnedCleanupImplementationResolution {
        .site = std::move(site),
    };
}

auto format_owned_cleanup_implementation_resolution(
    OwnedCleanupImplementationResolution const& resolution
) -> std::string {
    auto output = std::ostringstream {};
    output << (resolution.resolved ? "resolved " : "missing ");
    output << format_planned_drop_site(resolution.site);
    return output.str();
}

auto format_owned_cleanup_implementation_resolution_report(
    std::vector<OwnedCleanupSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(sites.size());
    for (auto const& site : sites) {
        report.push_back(format_owned_cleanup_implementation_resolution(resolve_owned_cleanup_implementation(site, implementations)));
    }
    return report;
}

auto owned_cleanup_implementation_blocker_reason_name(OwnedCleanupImplementationBlockerReason reason) -> std::string_view {
    switch (reason) {
    case OwnedCleanupImplementationBlockerReason::none:
        return "none";
    case OwnedCleanupImplementationBlockerReason::no_implementation_discovered:
        return "no implementation discovered";
    case OwnedCleanupImplementationBlockerReason::implementation_discovered_but_unproven:
        return "implementation discovered but unproven";
    }
    return "unknown";
}

auto diagnose_owned_cleanup_implementation(
    OwnedCleanupSite site,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> OwnedCleanupImplementationDiagnostic {
    auto matching_unproven_found = false;
    for (auto const& implementation : implementations) {
        if (implementation.source_type_name != site.source_type_name ||
            implementation.abi_symbol_name != site.abi_symbol_name) {
            continue;
        }
        if (implementation.proven) {
            return OwnedCleanupImplementationDiagnostic {
                .site = std::move(site),
                .resolved = true,
            };
        }
        matching_unproven_found = true;
    }
    return OwnedCleanupImplementationDiagnostic {
        .site = std::move(site),
        .blocker_reason = matching_unproven_found
            ? OwnedCleanupImplementationBlockerReason::implementation_discovered_but_unproven
            : OwnedCleanupImplementationBlockerReason::no_implementation_discovered,
    };
}

auto format_owned_cleanup_implementation_diagnostic(
    OwnedCleanupImplementationDiagnostic const& diagnostic
) -> std::string {
    auto output = std::ostringstream {};
    output << "drop diagnostic " << format_planned_drop_site(diagnostic.site);
    if (diagnostic.resolved) {
        output << " resolved";
    } else {
        output << " blocked " << owned_cleanup_implementation_blocker_reason_name(diagnostic.blocker_reason);
    }
    return output.str();
}

auto format_owned_cleanup_implementation_diagnostic_report(
    std::vector<OwnedCleanupSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(sites.size());
    for (auto const& site : sites) {
        report.push_back(format_owned_cleanup_implementation_diagnostic(diagnose_owned_cleanup_implementation(site, implementations)));
    }
    return report;
}

auto authorize_owned_cleanup_lowering(
    OwnedCleanupSite site,
    std::vector<OwnedCleanupImplementation> const& implementations,
    SemanticOwnedCleanupLoweringGate semantic_owned_cleanup_lowering_gate
) -> OwnedCleanupLoweringAuthorization {
    auto matching_implementation = std::find_if(
        implementations.begin(),
        implementations.end(),
        [&site](OwnedCleanupImplementation const& implementation) {
            return implementation.source_type_name == site.source_type_name &&
                   implementation.abi_symbol_name == site.abi_symbol_name &&
                   implementation.proven;
        }
    );
    auto semantic_resolved = matching_implementation != implementations.end();
    auto compiler_intrinsic_owned_cleanup =
        semantic_resolved && matching_implementation->origin == OwnedCleanupImplementationOrigin::compiler_intrinsic;
    auto semantic_owned_cleanup_lowering_enabled =
        semantic_owned_cleanup_lowering_gate == SemanticOwnedCleanupLoweringGate::enabled;
    return OwnedCleanupLoweringAuthorization {
        .site = std::move(site),
        .semantic_resolved = semantic_resolved,
        .semantic_owned_cleanup_lowering_enabled = semantic_owned_cleanup_lowering_enabled,
        .compiler_intrinsic_owned_cleanup = compiler_intrinsic_owned_cleanup,
        .authorized = semantic_resolved && (compiler_intrinsic_owned_cleanup || semantic_owned_cleanup_lowering_enabled),
    };
}

auto format_owned_cleanup_lowering_authorization(
    OwnedCleanupLoweringAuthorization const& authorization
) -> std::string {
    auto output = std::ostringstream {};
    output << "drop lowering authorization " << format_planned_drop_site(authorization.site);
    if (authorization.authorized) {
        if (authorization.compiler_intrinsic_owned_cleanup) {
            output << " semantic-resolved lowering-authorized compiler-owned cleanup accepted";
        } else {
            output << " semantic-resolved lowering-authorized semantic owned cleanup lowering accepted";
        }
    } else if (authorization.semantic_resolved) {
        output << " semantic-resolved lowering-blocked semantic owned cleanup lowering not accepted";
    } else {
        output << " semantic-unresolved lowering-blocked semantic drop unresolved";
    }
    return output.str();
}

auto authorize_owned_cleanup_lowerings(
    std::vector<OwnedCleanupSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations,
    SemanticOwnedCleanupLoweringGate semantic_owned_cleanup_lowering_gate
) -> std::vector<OwnedCleanupLoweringAuthorization> {
    auto authorizations = std::vector<OwnedCleanupLoweringAuthorization> {};
    authorizations.reserve(sites.size());
    for (auto const& site : sites) {
        authorizations.push_back(authorize_owned_cleanup_lowering(site, implementations, semantic_owned_cleanup_lowering_gate));
    }
    return authorizations;
}

auto format_owned_cleanup_lowering_authorization(
    OwnedCleanupSite const& site,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::string {
    return format_owned_cleanup_lowering_authorization(authorize_owned_cleanup_lowering(site, implementations));
}

auto format_owned_cleanup_lowering_authorization_report(
    std::vector<OwnedCleanupLoweringAuthorization> const& authorizations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(authorizations.size());
    for (auto const& authorization : authorizations) {
        report.push_back(format_owned_cleanup_lowering_authorization(authorization));
    }
    return report;
}

auto format_owned_cleanup_lowering_authorization_report(
    std::vector<OwnedCleanupSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::vector<std::string> {
    return format_owned_cleanup_lowering_authorization_report(authorize_owned_cleanup_lowerings(sites, implementations));
}

auto summarize_owned_cleanup_implementation_resolutions(
    std::vector<OwnedCleanupSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::vector<OwnedCleanupImplementationResolutionSummary> {
    auto summaries = std::vector<OwnedCleanupImplementationResolutionSummary> {};
    for (auto const& site : sites) {
        auto resolution = resolve_owned_cleanup_implementation(site, implementations);
        auto existing = std::find_if(
            summaries.begin(),
            summaries.end(),
            [&site](OwnedCleanupImplementationResolutionSummary const& summary) {
                return summary.source_type_name == site.source_type_name &&
                       summary.abi_symbol_name == site.abi_symbol_name;
            }
        );

        if (existing == summaries.end()) {
            summaries.push_back(OwnedCleanupImplementationResolutionSummary {
                .source_type_name = site.source_type_name,
                .abi_symbol_name = site.abi_symbol_name,
            });
            existing = summaries.end() - 1;
        }

        if (resolution.resolved) {
            ++existing->resolved_sites;
        } else {
            ++existing->missing_sites;
        }
    }
    return summaries;
}

auto format_owned_cleanup_implementation_resolution_summary(
    OwnedCleanupImplementationResolutionSummary const& summary
) -> std::string {
    auto output = std::ostringstream {};
    output << "drop resolution summary " << summary.abi_symbol_name;
    if (!summary.source_type_name.empty()) {
        output << " for " << summary.source_type_name;
    }
    output << " resolved " << summary.resolved_sites << " missing " << summary.missing_sites;
    return output.str();
}

auto format_owned_cleanup_implementation_resolution_summary_report(
    std::vector<OwnedCleanupImplementationResolutionSummary> const& summaries
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(summaries.size());
    for (auto const& summary : summaries) {
        report.push_back(format_owned_cleanup_implementation_resolution_summary(summary));
    }
    return report;
}

}  // namespace orison::semantics
