#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace orison::semantics {

enum class DropImplementationOrigin {
    source_derived,
    compiler_intrinsic,
    test_fixture,
};

struct DropImplementationBodySummary {
    bool finite = false;
    bool unsafe_boundary_required = false;
    std::vector<std::string> referenced_functions;
};

struct DropImplementation {
    std::string source_type_name;
    std::string abi_symbol_name;
    std::size_t declaration_line = 0;
    bool proven = false;
    DropImplementationOrigin origin = DropImplementationOrigin::source_derived;
    DropImplementationBodySummary body;
};

struct DropImplementationCandidate {
    std::string source_type_name;
    std::size_t declaration_line = 0;
    DropImplementationBodySummary body;
};

struct PlannedDropSite {
    std::string source_type_name;
    std::string abi_symbol_name;
    std::string owner_name;
    std::size_t site_line = 0;
};

struct DropImplementationResolution {
    PlannedDropSite site;
    bool resolved = false;
};

enum class DropImplementationBlockerReason {
    none,
    no_implementation_discovered,
    implementation_discovered_but_unproven,
};

struct DropImplementationDiagnostic {
    PlannedDropSite site;
    bool resolved = false;
    DropImplementationBlockerReason blocker_reason = DropImplementationBlockerReason::none;
};

struct DropImplementationResolutionSummary {
    std::string source_type_name;
    std::string abi_symbol_name;
    std::size_t resolved_sites = 0;
    std::size_t missing_sites = 0;
};

auto drop_abi_symbol_name(std::string_view source_type_name) -> std::string;

auto drop_implementation_origin_name(DropImplementationOrigin origin) -> std::string_view;

auto source_derived_drop_implementation(
    std::string source_type_name,
    std::size_t declaration_line,
    DropImplementationBodySummary body
) -> DropImplementation;

auto collect_source_derived_drop_implementations(
    std::vector<DropImplementationCandidate> const& candidates
) -> std::vector<DropImplementation>;

auto format_drop_implementation(DropImplementation const& implementation) -> std::string;

auto format_planned_drop_site(PlannedDropSite const& site) -> std::string;

auto format_planned_drop_site_report(std::vector<PlannedDropSite> const& sites) -> std::vector<std::string>;

auto resolve_drop_implementation(
    PlannedDropSite site,
    std::vector<DropImplementation> const& implementations
) -> DropImplementationResolution;

auto format_drop_implementation_resolution(
    DropImplementationResolution const& resolution
) -> std::string;

auto format_drop_implementation_resolution_report(
    std::vector<PlannedDropSite> const& sites,
    std::vector<DropImplementation> const& implementations
) -> std::vector<std::string>;

auto drop_implementation_blocker_reason_name(DropImplementationBlockerReason reason) -> std::string_view;

auto diagnose_drop_implementation(
    PlannedDropSite site,
    std::vector<DropImplementation> const& implementations
) -> DropImplementationDiagnostic;

auto format_drop_implementation_diagnostic(
    DropImplementationDiagnostic const& diagnostic
) -> std::string;

auto format_drop_implementation_diagnostic_report(
    std::vector<PlannedDropSite> const& sites,
    std::vector<DropImplementation> const& implementations
) -> std::vector<std::string>;

auto summarize_drop_implementation_resolutions(
    std::vector<PlannedDropSite> const& sites,
    std::vector<DropImplementation> const& implementations
) -> std::vector<DropImplementationResolutionSummary>;

auto format_drop_implementation_resolution_summary(
    DropImplementationResolutionSummary const& summary
) -> std::string;

auto format_drop_implementation_resolution_summary_report(
    std::vector<DropImplementationResolutionSummary> const& summaries
) -> std::vector<std::string>;

}  // namespace orison::semantics
