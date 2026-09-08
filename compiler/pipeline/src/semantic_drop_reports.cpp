#include "semantic_drop_reports.hpp"

#include "lowering_emission_options.hpp"

#include <algorithm>

namespace orison::pipeline {
namespace {

auto collect_discovered_drop_implementations(
    syntax::ModuleSyntax const& module,
    CompilePipelineOptions const& options,
    std::vector<semantics::PlannedDropSite> const& semantic_summary_drop_sites
) -> std::vector<SemanticDropImplementationDiscovery> {
    auto discovered_drop_implementations = std::vector<SemanticDropImplementationDiscovery> {};
    discovered_drop_implementations.reserve(
        options.test_only_semantic_drop_implementations.size() +
        options.test_only_semantic_drop_implementation_candidates.size() +
        semantic_summary_drop_sites.size()
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
    auto compiler_intrinsic_implementations = semantics::collect_compiler_intrinsic_owned_cleanup_implementations(
        semantic_summary_drop_sites,
        module
    );
    for (auto const& implementation : compiler_intrinsic_implementations) {
        auto const existing = std::find_if(
            discovered_drop_implementations.begin(),
            discovered_drop_implementations.end(),
            [&](SemanticDropImplementationDiscovery const& discovery) {
                return discovery.implementation.source_type_name == implementation.source_type_name &&
                       discovery.implementation.abi_symbol_name == implementation.abi_symbol_name;
            }
        );
        if (existing != discovered_drop_implementations.end()) {
            continue;
        }
        discovered_drop_implementations.push_back(SemanticDropImplementationDiscovery {
            .implementation = implementation,
            .discovery_name = "compiler-owned-cleanup",
        });
    }
    return discovered_drop_implementations;
}

}  // namespace

void populate_semantic_drop_reports(
    CompilePipelineResult& result,
    CompilePipelineOptions const& options
) {
    auto semantic_summary_drop_sites =
        semantics::project_semantic_drop_obligations(result.semantic_result.semantic_module);
    result.semantic_drop_state.discovered_implementations = collect_discovered_drop_implementations(
        result.parse_result.module,
        options,
        semantic_summary_drop_sites
    );
    auto semantic_drop_implementations = std::vector<semantics::DropImplementation> {};
    semantic_drop_implementations.reserve(result.semantic_drop_state.discovered_implementations.size());
    for (auto const& implementation : result.semantic_drop_state.discovered_implementations) {
        semantic_drop_implementations.push_back(implementation.implementation);
    }
    auto const source_drop_lowering_gate = source_drop_lowering_enabled(options)
                                              ? semantics::SourceDropLoweringGate::enabled
                                              : semantics::SourceDropLoweringGate::disabled;
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
