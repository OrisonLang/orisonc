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

auto drop_abi_symbol_name(std::string_view source_type_name) -> std::string {
    auto symbol = std::string {"__orison_drop."};
    for (auto character : source_type_name) {
        auto const allowed =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9');
        symbol.push_back(allowed ? character : '_');
    }
    return symbol;
}

auto drop_implementation_origin_name(DropImplementationOrigin origin) -> std::string_view {
    switch (origin) {
    case DropImplementationOrigin::source_derived:
        return "source-derived";
    case DropImplementationOrigin::compiler_intrinsic:
        return "compiler-intrinsic";
    case DropImplementationOrigin::test_fixture:
        return "test-fixture";
    }
    return "unknown";
}

auto source_derived_drop_implementation(
    std::string source_type_name,
    std::size_t declaration_line,
    DropImplementationBodySummary body
) -> DropImplementation {
    auto symbol_name = drop_abi_symbol_name(source_type_name);
    auto proven = body.finite;
    return DropImplementation {
        .source_type_name = std::move(source_type_name),
        .abi_symbol_name = std::move(symbol_name),
        .declaration_line = declaration_line,
        .proven = proven,
        .origin = DropImplementationOrigin::source_derived,
        .body = std::move(body),
    };
}

auto collect_source_derived_drop_implementations(
    std::vector<DropImplementationCandidate> const& candidates
) -> std::vector<DropImplementation> {
    auto implementations = std::vector<DropImplementation> {};
    for (auto const& candidate : candidates) {
        if (candidate.source_type_name.empty()) {
            continue;
        }
        auto symbol_name = drop_abi_symbol_name(candidate.source_type_name);
        auto existing = std::find_if(
            implementations.begin(),
            implementations.end(),
            [&candidate, &symbol_name](DropImplementation const& implementation) {
                return implementation.source_type_name == candidate.source_type_name &&
                       implementation.abi_symbol_name == symbol_name;
            }
        );
        if (existing != implementations.end()) {
            continue;
        }
        implementations.push_back(source_derived_drop_implementation(
            candidate.source_type_name,
            candidate.declaration_line,
            candidate.body
        ));
    }
    return implementations;
}

auto prove_source_derived_drop_implementation_body(
    syntax::ImplementationSyntax const& implementation
) -> DropImplementationBodySummary {
    auto method = std::find_if(
        implementation.methods.begin(),
        implementation.methods.end(),
        [](syntax::FunctionSyntax const& candidate) {
            return candidate.name == "drop";
        }
    );
    if (method == implementation.methods.end()) {
        return DropImplementationBodySummary {};
    }
    if (method->body_statements.empty()) {
        return DropImplementationBodySummary {
            .finite = true,
        };
    }
    if (method->body_statements.size() == 1 && is_naked_return(method->body_statements.front())) {
        return DropImplementationBodySummary {
            .finite = true,
        };
    }
    return DropImplementationBodySummary {};
}

auto collect_source_derived_drop_implementation_candidates(
    syntax::ModuleSyntax const& module
) -> std::vector<DropImplementationCandidate> {
    auto candidates = std::vector<DropImplementationCandidate> {};
    for (auto const& implementation : module.implementations) {
        if (!is_drop_interface(implementation.interface_type) || !has_drop_method(implementation)) {
            continue;
        }
        candidates.push_back(DropImplementationCandidate {
            .source_type_name = render_source_type_name(implementation.receiver_type),
            .declaration_line = drop_method_line(implementation),
            .body = prove_source_derived_drop_implementation_body(implementation),
        });
    }
    return candidates;
}

auto format_drop_implementation(DropImplementation const& implementation) -> std::string {
    auto output = std::ostringstream {};
    output << "drop implementation " << implementation.abi_symbol_name;
    if (!implementation.source_type_name.empty()) {
        output << " for " << implementation.source_type_name;
    }
    if (implementation.declaration_line > 0) {
        output << " declared at line " << implementation.declaration_line;
    }
    output << " origin " << drop_implementation_origin_name(implementation.origin);
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

auto format_planned_drop_site(PlannedDropSite const& site) -> std::string {
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

auto format_planned_drop_site_report(std::vector<PlannedDropSite> const& sites) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(sites.size());
    for (auto const& site : sites) {
        report.push_back(format_planned_drop_site(site));
    }
    return report;
}

auto resolve_drop_implementation(
    PlannedDropSite site,
    std::vector<DropImplementation> const& implementations
) -> DropImplementationResolution {
    for (auto const& implementation : implementations) {
        if (implementation.source_type_name == site.source_type_name &&
            implementation.abi_symbol_name == site.abi_symbol_name &&
            implementation.proven) {
            return DropImplementationResolution {
                .site = std::move(site),
                .resolved = true,
            };
        }
    }
    return DropImplementationResolution {
        .site = std::move(site),
    };
}

auto format_drop_implementation_resolution(
    DropImplementationResolution const& resolution
) -> std::string {
    auto output = std::ostringstream {};
    output << (resolution.resolved ? "resolved " : "missing ");
    output << format_planned_drop_site(resolution.site);
    return output.str();
}

auto format_drop_implementation_resolution_report(
    std::vector<PlannedDropSite> const& sites,
    std::vector<DropImplementation> const& implementations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(sites.size());
    for (auto const& site : sites) {
        report.push_back(format_drop_implementation_resolution(resolve_drop_implementation(site, implementations)));
    }
    return report;
}

auto drop_implementation_blocker_reason_name(DropImplementationBlockerReason reason) -> std::string_view {
    switch (reason) {
    case DropImplementationBlockerReason::none:
        return "none";
    case DropImplementationBlockerReason::no_implementation_discovered:
        return "no implementation discovered";
    case DropImplementationBlockerReason::implementation_discovered_but_unproven:
        return "implementation discovered but unproven";
    }
    return "unknown";
}

auto diagnose_drop_implementation(
    PlannedDropSite site,
    std::vector<DropImplementation> const& implementations
) -> DropImplementationDiagnostic {
    auto matching_unproven_found = false;
    for (auto const& implementation : implementations) {
        if (implementation.source_type_name != site.source_type_name ||
            implementation.abi_symbol_name != site.abi_symbol_name) {
            continue;
        }
        if (implementation.proven) {
            return DropImplementationDiagnostic {
                .site = std::move(site),
                .resolved = true,
            };
        }
        matching_unproven_found = true;
    }
    return DropImplementationDiagnostic {
        .site = std::move(site),
        .blocker_reason = matching_unproven_found
            ? DropImplementationBlockerReason::implementation_discovered_but_unproven
            : DropImplementationBlockerReason::no_implementation_discovered,
    };
}

auto format_drop_implementation_diagnostic(
    DropImplementationDiagnostic const& diagnostic
) -> std::string {
    auto output = std::ostringstream {};
    output << "drop diagnostic " << format_planned_drop_site(diagnostic.site);
    if (diagnostic.resolved) {
        output << " resolved";
    } else {
        output << " blocked " << drop_implementation_blocker_reason_name(diagnostic.blocker_reason);
    }
    return output.str();
}

auto format_drop_implementation_diagnostic_report(
    std::vector<PlannedDropSite> const& sites,
    std::vector<DropImplementation> const& implementations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(sites.size());
    for (auto const& site : sites) {
        report.push_back(format_drop_implementation_diagnostic(diagnose_drop_implementation(site, implementations)));
    }
    return report;
}

auto authorize_drop_lowering(
    PlannedDropSite site,
    std::vector<DropImplementation> const& implementations,
    SourceDropLoweringGate source_drop_lowering_gate
) -> DropLoweringAuthorization {
    auto semantic_resolved = resolve_drop_implementation(site, implementations).resolved;
    auto source_drop_lowering_enabled = source_drop_lowering_gate == SourceDropLoweringGate::enabled;
    return DropLoweringAuthorization {
        .site = std::move(site),
        .semantic_resolved = semantic_resolved,
        .source_drop_lowering_enabled = source_drop_lowering_enabled,
        .authorized = semantic_resolved && source_drop_lowering_enabled,
    };
}

auto format_drop_lowering_authorization(
    DropLoweringAuthorization const& authorization
) -> std::string {
    auto output = std::ostringstream {};
    output << "drop lowering authorization " << format_planned_drop_site(authorization.site);
    if (authorization.authorized) {
        output << " semantic-resolved lowering-authorized source drop lowering accepted";
    } else if (authorization.semantic_resolved) {
        output << " semantic-resolved lowering-blocked source drop lowering not accepted";
    } else {
        output << " semantic-unresolved lowering-blocked semantic drop unresolved";
    }
    return output.str();
}

auto authorize_drop_lowerings(
    std::vector<PlannedDropSite> const& sites,
    std::vector<DropImplementation> const& implementations,
    SourceDropLoweringGate source_drop_lowering_gate
) -> std::vector<DropLoweringAuthorization> {
    auto authorizations = std::vector<DropLoweringAuthorization> {};
    authorizations.reserve(sites.size());
    for (auto const& site : sites) {
        authorizations.push_back(authorize_drop_lowering(site, implementations, source_drop_lowering_gate));
    }
    return authorizations;
}

auto format_drop_lowering_authorization(
    PlannedDropSite const& site,
    std::vector<DropImplementation> const& implementations
) -> std::string {
    return format_drop_lowering_authorization(authorize_drop_lowering(site, implementations));
}

auto format_drop_lowering_authorization_report(
    std::vector<DropLoweringAuthorization> const& authorizations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(authorizations.size());
    for (auto const& authorization : authorizations) {
        report.push_back(format_drop_lowering_authorization(authorization));
    }
    return report;
}

auto format_drop_lowering_authorization_report(
    std::vector<PlannedDropSite> const& sites,
    std::vector<DropImplementation> const& implementations
) -> std::vector<std::string> {
    return format_drop_lowering_authorization_report(authorize_drop_lowerings(sites, implementations));
}

auto summarize_drop_implementation_resolutions(
    std::vector<PlannedDropSite> const& sites,
    std::vector<DropImplementation> const& implementations
) -> std::vector<DropImplementationResolutionSummary> {
    auto summaries = std::vector<DropImplementationResolutionSummary> {};
    for (auto const& site : sites) {
        auto resolution = resolve_drop_implementation(site, implementations);
        auto existing = std::find_if(
            summaries.begin(),
            summaries.end(),
            [&site](DropImplementationResolutionSummary const& summary) {
                return summary.source_type_name == site.source_type_name &&
                       summary.abi_symbol_name == site.abi_symbol_name;
            }
        );

        if (existing == summaries.end()) {
            summaries.push_back(DropImplementationResolutionSummary {
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

auto format_drop_implementation_resolution_summary(
    DropImplementationResolutionSummary const& summary
) -> std::string {
    auto output = std::ostringstream {};
    output << "drop resolution summary " << summary.abi_symbol_name;
    if (!summary.source_type_name.empty()) {
        output << " for " << summary.source_type_name;
    }
    output << " resolved " << summary.resolved_sites << " missing " << summary.missing_sites;
    return output.str();
}

auto format_drop_implementation_resolution_summary_report(
    std::vector<DropImplementationResolutionSummary> const& summaries
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(summaries.size());
    for (auto const& summary : summaries) {
        report.push_back(format_drop_implementation_resolution_summary(summary));
    }
    return report;
}

}  // namespace orison::semantics
