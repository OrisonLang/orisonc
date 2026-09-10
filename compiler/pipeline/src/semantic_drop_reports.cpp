#include "semantic_drop_reports.hpp"

#include "lowering_emission_options.hpp"

#include <algorithm>

namespace orison::pipeline {
namespace {

auto collect_discovered_owned_cleanup_implementations(
    syntax::ModuleSyntax const& module,
    CompilePipelineOptions const& options,
    std::vector<semantics::OwnedCleanupSite> const& semantic_summary_drop_sites
) -> std::vector<SemanticOwnedCleanupImplementationDiscovery> {
    auto discovered_owned_cleanup_implementations = std::vector<SemanticOwnedCleanupImplementationDiscovery> {};
    discovered_owned_cleanup_implementations.reserve(
        options.test_only_semantic_owned_cleanup_implementations.size() +
        options.test_only_semantic_owned_cleanup_implementation_candidates.size() +
        semantic_summary_drop_sites.size()
    );
    for (auto const& implementation : options.test_only_semantic_owned_cleanup_implementations) {
        discovered_owned_cleanup_implementations.push_back(SemanticOwnedCleanupImplementationDiscovery {
            .implementation = implementation,
            .discovery_name = "test-injection",
        });
    }
    auto semantic_candidate_implementations = semantics::collect_semantic_owned_cleanup_implementations(
        options.test_only_semantic_owned_cleanup_implementation_candidates
    );
    for (auto const& implementation : semantic_candidate_implementations) {
        discovered_owned_cleanup_implementations.push_back(SemanticOwnedCleanupImplementationDiscovery {
            .implementation = implementation,
            .discovery_name = "semantic-candidate",
        });
    }
    auto compiler_intrinsic_implementations = semantics::collect_compiler_intrinsic_owned_cleanup_implementations(
        semantic_summary_drop_sites,
        module
    );
    for (auto const& implementation : compiler_intrinsic_implementations) {
        auto const existing = std::find_if(
            discovered_owned_cleanup_implementations.begin(),
            discovered_owned_cleanup_implementations.end(),
            [&](SemanticOwnedCleanupImplementationDiscovery const& discovery) {
                return discovery.implementation.source_type_name == implementation.source_type_name &&
                       discovery.implementation.abi_symbol_name == implementation.abi_symbol_name;
            }
        );
        if (existing != discovered_owned_cleanup_implementations.end()) {
            continue;
        }
        discovered_owned_cleanup_implementations.push_back(SemanticOwnedCleanupImplementationDiscovery {
            .implementation = implementation,
            .discovery_name = "compiler-owned-cleanup",
        });
    }
    return discovered_owned_cleanup_implementations;
}

}  // namespace

void populate_semantic_drop_reports(
    CompilePipelineResult& result,
    CompilePipelineOptions const& options
) {
    auto semantic_summary_drop_sites =
        semantics::project_semantic_drop_obligations(result.semantic_result.semantic_module);
    result.semantic_owned_cleanup_state.discovered_implementations = collect_discovered_owned_cleanup_implementations(
        result.parse_result.module,
        options,
        semantic_summary_drop_sites
    );
    auto semantic_owned_cleanup_implementations = std::vector<semantics::OwnedCleanupImplementation> {};
    semantic_owned_cleanup_implementations.reserve(result.semantic_owned_cleanup_state.discovered_implementations.size());
    for (auto const& implementation : result.semantic_owned_cleanup_state.discovered_implementations) {
        semantic_owned_cleanup_implementations.push_back(implementation.implementation);
    }
    auto const semantic_owned_cleanup_lowering_gate = semantic_owned_cleanup_lowering_enabled(options)
                                                       ? semantics::SemanticOwnedCleanupLoweringGate::enabled
                                                       : semantics::SemanticOwnedCleanupLoweringGate::disabled;
    result.semantic_owned_cleanup_lowering_authorizations = semantics::authorize_owned_cleanup_lowerings(
        semantic_summary_drop_sites,
        semantic_owned_cleanup_implementations,
        semantic_owned_cleanup_lowering_gate
    );
    result.semantic_owned_cleanup_state.resolution_summaries = semantics::summarize_owned_cleanup_implementation_resolutions(
        semantic_summary_drop_sites,
        semantic_owned_cleanup_implementations
    );
}

}  // namespace orison::pipeline
