#include "semantic_drop_reports.hpp"

#include "lowering_emission_options.hpp"

#include <vector>

namespace orison::pipeline {
namespace {

auto collect_discovered_drop_implementations(
    syntax::ModuleSyntax const& module,
    CompilePipelineOptions const& options
) -> std::vector<SemanticDropImplementationDiscovery> {
    auto discovered_drop_implementations = std::vector<SemanticDropImplementationDiscovery> {};
    auto parsed_semantic_drop_implementation_candidates =
        semantics::collect_source_derived_drop_implementation_candidates(module);
    discovered_drop_implementations.reserve(
        options.test_only_semantic_drop_implementations.size() +
        options.test_only_semantic_drop_implementation_candidates.size() +
        parsed_semantic_drop_implementation_candidates.size()
    );
    for (auto const& implementation : options.test_only_semantic_drop_implementations) {
        discovered_drop_implementations.push_back(SemanticDropImplementationDiscovery {
            .implementation = implementation,
            .discovery_name = "test-injection",
        });
    }
    auto source_derived_implementations = semantics::collect_source_derived_drop_implementations(
        options.test_only_semantic_drop_implementation_candidates
    );
    for (auto const& implementation : source_derived_implementations) {
        discovered_drop_implementations.push_back(SemanticDropImplementationDiscovery {
            .implementation = implementation,
            .discovery_name = "candidate-collection",
        });
    }
    auto parsed_source_derived_implementations = semantics::collect_source_derived_drop_implementations(
        parsed_semantic_drop_implementation_candidates
    );
    for (auto const& implementation : parsed_source_derived_implementations) {
        discovered_drop_implementations.push_back(SemanticDropImplementationDiscovery {
            .implementation = implementation,
            .discovery_name = "parsed-candidate-collection",
        });
    }
    return discovered_drop_implementations;
}

auto planned_drop_sites_from_semantic_summary(
    semantics::SemanticModuleSummary const& summary
) -> std::vector<semantics::PlannedDropSite> {
    auto sites = std::vector<semantics::PlannedDropSite> {};
    sites.reserve(summary.drop_obligations.size());
    for (auto const& obligation : summary.drop_obligations) {
        sites.push_back(semantics::PlannedDropSite {
            .source_type_name = obligation.source_type_name,
            .abi_symbol_name = obligation.abi_symbol_name,
            .owner_name = obligation.owner_name,
            .site_line = obligation.line,
        });
    }
    return sites;
}

}  // namespace

void populate_semantic_drop_reports(
    CompilePipelineResult& result,
    CompilePipelineOptions const& options
) {
    result.semantic_drop_state.discovered_implementations =
        collect_discovered_drop_implementations(result.parse_result.module, options);
    auto semantic_drop_implementations = std::vector<semantics::DropImplementation> {};
    semantic_drop_implementations.reserve(result.semantic_drop_state.discovered_implementations.size());
    for (auto const& implementation : result.semantic_drop_state.discovered_implementations) {
        semantic_drop_implementations.push_back(implementation.implementation);
    }
    auto const source_drop_lowering_gate = source_drop_lowering_enabled(options)
                                              ? semantics::SourceDropLoweringGate::enabled
                                              : semantics::SourceDropLoweringGate::disabled;
    auto semantic_summary_drop_sites =
        planned_drop_sites_from_semantic_summary(result.semantic_result.semantic_module);
    result.semantic_drop_lowering_authorizations = semantics::authorize_drop_lowerings(
        semantic_summary_drop_sites,
        semantic_drop_implementations,
        source_drop_lowering_gate
    );
    result.semantic_drop_state.resolution_summaries = semantics::summarize_drop_implementation_resolutions(
        semantic_summary_drop_sites,
        semantic_drop_implementations
    );
}

}  // namespace orison::pipeline
