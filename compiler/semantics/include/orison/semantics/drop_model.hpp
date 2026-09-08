#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace orison::syntax {
struct ImplementationSyntax;
struct ModuleSyntax;
}

namespace orison::semantics {

enum class OwnedCleanupImplementationOrigin {
    source_derived,
    compiler_intrinsic,
    test_fixture,
};

struct OwnedCleanupImplementationBodySummary {
    bool finite = false;
    bool unsafe_boundary_required = false;
    std::vector<std::string> referenced_functions;
};

struct OwnedCleanupImplementation {
    std::string source_type_name;
    std::string abi_symbol_name;
    std::size_t declaration_line = 0;
    bool proven = false;
    OwnedCleanupImplementationOrigin origin = OwnedCleanupImplementationOrigin::source_derived;
    OwnedCleanupImplementationBodySummary body;
};

struct OwnedCleanupImplementationCandidate {
    std::string source_type_name;
    std::size_t declaration_line = 0;
    OwnedCleanupImplementationBodySummary body;
};

struct PlannedDropSite {
    std::string source_type_name;
    std::string abi_symbol_name;
    std::string owner_name;
    std::size_t site_line = 0;
};

struct OwnedCleanupImplementationResolution {
    PlannedDropSite site;
    bool resolved = false;
};

enum class OwnedCleanupImplementationBlockerReason {
    none,
    no_implementation_discovered,
    implementation_discovered_but_unproven,
};

struct OwnedCleanupImplementationDiagnostic {
    PlannedDropSite site;
    bool resolved = false;
    OwnedCleanupImplementationBlockerReason blocker_reason = OwnedCleanupImplementationBlockerReason::none;
};

struct OwnedCleanupImplementationResolutionSummary {
    std::string source_type_name;
    std::string abi_symbol_name;
    std::size_t resolved_sites = 0;
    std::size_t missing_sites = 0;
};

enum class SourceDropLoweringGate {
    disabled,
    enabled,
};

struct OwnedCleanupLoweringAuthorization {
    PlannedDropSite site;
    bool semantic_resolved = false;
    bool source_drop_lowering_enabled = false;
    bool compiler_intrinsic_owned_cleanup = false;
    bool authorized = false;
};

auto drop_abi_symbol_name(std::string_view source_type_name) -> std::string;

auto owned_cleanup_implementation_origin_name(OwnedCleanupImplementationOrigin origin) -> std::string_view;

auto source_derived_owned_cleanup_implementation(
    std::string source_type_name,
    std::size_t declaration_line,
    OwnedCleanupImplementationBodySummary body
) -> OwnedCleanupImplementation;

auto compiler_intrinsic_owned_cleanup_implementation(
    std::string source_type_name,
    std::size_t declaration_line
) -> OwnedCleanupImplementation;

auto collect_source_derived_owned_cleanup_implementations(
    std::vector<OwnedCleanupImplementationCandidate> const& candidates
) -> std::vector<OwnedCleanupImplementation>;

auto prove_source_derived_owned_cleanup_implementation_body(
    syntax::ImplementationSyntax const& implementation
) -> OwnedCleanupImplementationBodySummary;

auto collect_source_derived_owned_cleanup_implementation_candidates(
    syntax::ModuleSyntax const& module
) -> std::vector<OwnedCleanupImplementationCandidate>;

auto collect_compiler_intrinsic_owned_cleanup_implementations(
    std::vector<PlannedDropSite> const& sites,
    syntax::ModuleSyntax const& module
) -> std::vector<OwnedCleanupImplementation>;

auto format_owned_cleanup_implementation(OwnedCleanupImplementation const& implementation) -> std::string;

auto format_planned_drop_site(PlannedDropSite const& site) -> std::string;

auto format_planned_drop_site_report(std::vector<PlannedDropSite> const& sites) -> std::vector<std::string>;

auto resolve_owned_cleanup_implementation(
    PlannedDropSite site,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> OwnedCleanupImplementationResolution;

auto format_owned_cleanup_implementation_resolution(
    OwnedCleanupImplementationResolution const& resolution
) -> std::string;

auto format_owned_cleanup_implementation_resolution_report(
    std::vector<PlannedDropSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::vector<std::string>;

auto owned_cleanup_implementation_blocker_reason_name(OwnedCleanupImplementationBlockerReason reason) -> std::string_view;

auto diagnose_owned_cleanup_implementation(
    PlannedDropSite site,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> OwnedCleanupImplementationDiagnostic;

auto format_owned_cleanup_implementation_diagnostic(
    OwnedCleanupImplementationDiagnostic const& diagnostic
) -> std::string;

auto format_owned_cleanup_implementation_diagnostic_report(
    std::vector<PlannedDropSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::vector<std::string>;

auto authorize_owned_cleanup_lowering(
    PlannedDropSite site,
    std::vector<OwnedCleanupImplementation> const& implementations,
    SourceDropLoweringGate source_drop_lowering_gate = SourceDropLoweringGate::disabled
) -> OwnedCleanupLoweringAuthorization;

auto authorize_owned_cleanup_lowerings(
    std::vector<PlannedDropSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations,
    SourceDropLoweringGate source_drop_lowering_gate = SourceDropLoweringGate::disabled
) -> std::vector<OwnedCleanupLoweringAuthorization>;

auto format_owned_cleanup_lowering_authorization(
    OwnedCleanupLoweringAuthorization const& authorization
) -> std::string;

auto format_owned_cleanup_lowering_authorization(
    PlannedDropSite const& site,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::string;

auto format_owned_cleanup_lowering_authorization_report(
    std::vector<OwnedCleanupLoweringAuthorization> const& authorizations
) -> std::vector<std::string>;

auto format_owned_cleanup_lowering_authorization_report(
    std::vector<PlannedDropSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::vector<std::string>;

auto summarize_owned_cleanup_implementation_resolutions(
    std::vector<PlannedDropSite> const& sites,
    std::vector<OwnedCleanupImplementation> const& implementations
) -> std::vector<OwnedCleanupImplementationResolutionSummary>;

auto format_owned_cleanup_implementation_resolution_summary(
    OwnedCleanupImplementationResolutionSummary const& summary
) -> std::string;

auto format_owned_cleanup_implementation_resolution_summary_report(
    std::vector<OwnedCleanupImplementationResolutionSummary> const& summaries
) -> std::vector<std::string>;

}  // namespace orison::semantics
