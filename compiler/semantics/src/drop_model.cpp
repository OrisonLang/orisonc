#include "orison/semantics/drop_model.hpp"

#include <sstream>
#include <string_view>
#include <utility>

namespace orison::semantics {

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

}  // namespace orison::semantics
