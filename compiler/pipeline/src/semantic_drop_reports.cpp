#include "semantic_drop_reports.hpp"

#include "lowering_emission_options.hpp"

#include <string_view>
#include <vector>

namespace orison::pipeline {
namespace {

struct DiscoveredDropImplementation {
    semantics::DropImplementation implementation;
    std::string_view discovery;
};

auto format_discovered_drop_implementation(DiscoveredDropImplementation const& implementation) -> std::string {
    return semantics::format_drop_implementation(implementation.implementation) +
           " discovery " + std::string(implementation.discovery);
}

}  // namespace

void populate_semantic_drop_reports(
    CompilePipelineResult& result,
    CompilePipelineOptions const& options
) {
    auto discovered_drop_implementations = std::vector<DiscoveredDropImplementation> {};
    auto parsed_semantic_drop_implementation_candidates =
        semantics::collect_source_derived_drop_implementation_candidates(result.parse_result.module);
    discovered_drop_implementations.reserve(
        options.test_only_semantic_drop_implementations.size() +
        options.test_only_semantic_drop_implementation_candidates.size() +
        parsed_semantic_drop_implementation_candidates.size()
    );
    for (auto const& implementation : options.test_only_semantic_drop_implementations) {
        discovered_drop_implementations.push_back(DiscoveredDropImplementation {
            .implementation = implementation,
            .discovery = "test-injection",
        });
    }
    auto source_derived_implementations = semantics::collect_source_derived_drop_implementations(
        options.test_only_semantic_drop_implementation_candidates
    );
    for (auto const& implementation : source_derived_implementations) {
        discovered_drop_implementations.push_back(DiscoveredDropImplementation {
            .implementation = implementation,
            .discovery = "candidate-collection",
        });
    }
    auto parsed_source_derived_implementations = semantics::collect_source_derived_drop_implementations(
        parsed_semantic_drop_implementation_candidates
    );
    for (auto const& implementation : parsed_source_derived_implementations) {
        discovered_drop_implementations.push_back(DiscoveredDropImplementation {
            .implementation = implementation,
            .discovery = "parsed-candidate-collection",
        });
    }
    auto semantic_drop_implementations = std::vector<semantics::DropImplementation> {};
    semantic_drop_implementations.reserve(discovered_drop_implementations.size());
    for (auto const& implementation : discovered_drop_implementations) {
        semantic_drop_implementations.push_back(implementation.implementation);
    }
    result.semantic_dynamic_array_descriptor_origin_report =
        semantics::format_dynamic_array_descriptor_origin_report(
            result.semantic_result.dynamic_array_descriptor_origins
        );
    result.semantic_planned_drop_report =
        semantics::format_planned_drop_site_report(result.semantic_result.planned_drop_sites);
    result.semantic_drop_implementation_report.reserve(discovered_drop_implementations.size());
    for (auto const& implementation : discovered_drop_implementations) {
        result.semantic_drop_implementation_report.push_back(format_discovered_drop_implementation(implementation));
    }
    result.semantic_drop_resolution_report = semantics::format_drop_implementation_resolution_report(
        result.semantic_result.planned_drop_sites,
        semantic_drop_implementations
    );
    result.semantic_drop_diagnostic_report = semantics::format_drop_implementation_diagnostic_report(
        result.semantic_result.planned_drop_sites,
        semantic_drop_implementations
    );
    auto const source_drop_lowering_gate = source_drop_lowering_enabled(options)
                                              ? semantics::SourceDropLoweringGate::enabled
                                              : semantics::SourceDropLoweringGate::disabled;
    result.semantic_drop_lowering_authorizations = semantics::authorize_drop_lowerings(
        result.semantic_result.planned_drop_sites,
        semantic_drop_implementations,
        source_drop_lowering_gate
    );
    result.semantic_drop_lowering_authorization_report =
        semantics::format_drop_lowering_authorization_report(result.semantic_drop_lowering_authorizations);
    result.semantic_drop_resolution_summary_report = semantics::format_drop_implementation_resolution_summary_report(
        semantics::summarize_drop_implementation_resolutions(
            result.semantic_result.planned_drop_sites,
            semantic_drop_implementations
        )
    );
}

}  // namespace orison::pipeline
