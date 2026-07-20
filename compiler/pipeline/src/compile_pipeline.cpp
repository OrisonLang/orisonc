#include "orison/pipeline/compile_pipeline.hpp"

#include "orison/lowering/llvm_ir_emitter.hpp"
#include "orison/lowering/llvm_object_emitter.hpp"
#include "orison/pipeline/dynamic_array_cleanup_metadata.hpp"
#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include "lowering_emission_options.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>
#include <utility>

namespace orison::pipeline {
namespace {

struct DiscoveredDropImplementation {
    semantics::DropImplementation implementation;
    std::string_view discovery;
};

auto unquoted_text(std::string_view text) -> std::string {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return std::string(text.substr(1, text.size() - 2));
    }
    return std::string(text);
}

void collect_link_libraries(
    syntax::ModuleSyntax const& module,
    std::vector<std::string>& libraries
) {
    for (auto const& foreign_import : module.foreign_imports) {
        if (unquoted_text(foreign_import.abi) != "c" || foreign_import.library_name.empty()) {
            continue;
        }
        auto library = unquoted_text(foreign_import.library_name);
        if (std::find(libraries.begin(), libraries.end(), library) == libraries.end()) {
            libraries.push_back(std::move(library));
        }
    }
}

auto format_discovered_drop_implementation(DiscoveredDropImplementation const& implementation) -> std::string {
    return semantics::format_drop_implementation(implementation.implementation) +
           " discovery " + std::string(implementation.discovery);
}

auto report_contains(std::vector<std::string> const& lines, std::string_view fragment) -> bool {
    return std::ranges::any_of(lines, [&](auto const& line) {
        return line.find(fragment) != std::string::npos;
    });
}

auto plan_dynamic_array_cleanup_production_readiness(
    CompilePipelineResult const& result,
    CompilePipelineOptions const& options
) -> DynamicArrayCleanupProductionReadiness {
    return DynamicArrayCleanupProductionReadiness {
        .descriptor_origins_available = !result.semantic_dynamic_array_descriptor_origin_report.empty(),
        .descriptor_cleanup_plans_available = !result.dynamic_array_descriptor_cleanup_plan_report.empty(),
        .cleanup_obligations_available = !result.dynamic_array_cleanup_obligation_report.empty(),
        .sequence_verification_available = !result.dynamic_array_cleanup_sequence_verification_report.empty(),
        .sequence_verification_passed =
            !result.dynamic_array_cleanup_sequence_verification_report.empty() &&
            !report_contains(result.dynamic_array_cleanup_sequence_verification_report, " failed"),
        .cleanup_capability_proven =
            report_contains(result.dynamic_array_cleanup_emission_capability_report, "capability proven"),
        .production_signature_lowering_enabled =
            dynamic_array_parameter_lowering_enabled(options),
        .production_construction_lowering_enabled =
            dynamic_array_construction_lowering_enabled(options),
        .production_cleanup_emission_enabled =
            dynamic_array_cleanup_emission_enabled(options),
    };
}

}  // namespace

auto CompilePipelineResult::has_errors() const -> bool {
    return !error_text.empty();
}

auto dynamic_array_cleanup_production_ready(
    DynamicArrayCleanupProductionReadiness const& readiness
) -> bool {
    return readiness.descriptor_origins_available &&
        readiness.descriptor_cleanup_plans_available &&
        readiness.cleanup_obligations_available &&
        readiness.sequence_verification_available &&
        readiness.sequence_verification_passed &&
        readiness.cleanup_capability_proven &&
        readiness.production_signature_lowering_enabled &&
        readiness.production_construction_lowering_enabled &&
        readiness.production_cleanup_emission_enabled;
}

auto format_dynamic_array_cleanup_production_readiness(
    DynamicArrayCleanupProductionReadiness const& readiness
) -> std::string {
    auto const status = [](bool value) {
        return value ? "ok" : "missing";
    };
    auto output = std::ostringstream {};
    output << "dynamic array cleanup production readiness ";
    output << (dynamic_array_cleanup_production_ready(readiness) ? "ready" : "blocked");
    output << " [descriptor origins " << status(readiness.descriptor_origins_available) << "]";
    output << " [cleanup plans " << status(readiness.descriptor_cleanup_plans_available) << "]";
    output << " [cleanup obligations " << status(readiness.cleanup_obligations_available) << "]";
    output << " [sequence verification " << status(readiness.sequence_verification_available) << "]";
    output << " [sequence passed " << status(readiness.sequence_verification_passed) << "]";
    output << " [cleanup capability " << status(readiness.cleanup_capability_proven) << "]";
    output << " [production signatures " << status(readiness.production_signature_lowering_enabled) << "]";
    output << " [production construction " << status(readiness.production_construction_lowering_enabled) << "]";
    output << " [production cleanup emission " << status(readiness.production_cleanup_emission_enabled) << "]";
    output << " (metadata only)";
    return output.str();
}

void copy_lowering_emission_reports(
    CompilePipelineResult& result,
    lowering::LlvmIrEmissionResult&& emission,
    CompilePipelineOptions const& options
) {
    result.ir_text = std::move(emission.ir_text);
    result.dynamic_array_construction_plan_report =
        emission.dynamic_array_construction_plan_report();
    result.dynamic_array_runtime_request_report =
        emission.dynamic_array_runtime_request_report();
    result.dynamic_array_allocation_call_ir =
        std::move(emission.dynamic_array_allocation_call_ir);
    result.dynamic_array_descriptor_cleanup_plan_report =
        emission.dynamic_array_descriptor_cleanup_plan_report();
    result.dynamic_array_cleanup_obligation_report =
        emission.dynamic_array_cleanup_obligation_report();
    result.dynamic_array_cleanup_sequence_plan_report =
        emission.dynamic_array_cleanup_sequence_plan_report();
    result.dynamic_array_cleanup_sequence_verification_report =
        emission.dynamic_array_cleanup_sequence_verification_report();
    result.dynamic_array_cleanup_emission_gate_report =
        emission.dynamic_array_cleanup_emission_gate_report();
    result.dynamic_array_cleanup_emission_capability_report =
        emission.dynamic_array_cleanup_emission_capability_report();
    result.dynamic_array_cleanup_production_readiness =
        plan_dynamic_array_cleanup_production_readiness(result, options);
    result.dynamic_array_cleanup_production_readiness_report = {
        format_dynamic_array_cleanup_production_readiness(result.dynamic_array_cleanup_production_readiness),
    };
    result.planned_drop_report = emission.planned_drop_report();
    result.emitted_drop_declaration_report =
        emission.emitted_drop_declaration_report();
    result.planned_drop_action_report =
        emission.planned_drop_action_report();
    result.drop_cleanup_authorization_report =
        emission.drop_cleanup_authorization_report();
    result.drop_readiness_snapshot = emission.drop_readiness_snapshot();
    result.drop_readiness_snapshot_report =
        emission.drop_readiness_snapshot_report();
    result.drop_readiness_summary = emission.drop_readiness_summary();
    result.drop_readiness_summary_report =
        emission.drop_readiness_summary_report();
    result.drop_readiness_relation_report =
        emission.drop_readiness_relation_report();
    result.drop_readiness_blocker_summary =
        lowering::summarize_drop_readiness_blockers(result.drop_readiness_snapshot);
    result.drop_readiness_blocker_report =
        lowering::format_drop_readiness_blocker_report(result.drop_readiness_blocker_summary);
    result.drop_readiness_source_correlation_report =
        format_drop_readiness_source_correlation_report(result.drop_readiness_snapshot);
    result.semantic_drop_lowering_authorizations = std::move(emission.semantic_drop_lowering_authorizations);
}

auto CompilePipeline::analyze(std::filesystem::path const& source_path) const -> CompilePipelineResult {
    return analyze(source_path, CompilePipelineOptions {});
}

auto CompilePipeline::analyze(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    auto result = CompilePipelineResult {};
    result.source_file = source::SourceFile::read(source_path);
    if (!result.source_file.has_value()) {
        result.error_text = "error: unable to read source file\n";
        return result;
    }

    syntax::ModuleParser parser;
    result.parse_result = parser.parse(*result.source_file);
    if (result.parse_result.diagnostics.has_errors()) {
        result.error_text = result.parse_result.diagnostics.render(result.source_file->path().string());
        return result;
    }
    collect_link_libraries(result.parse_result.module, result.link_libraries);

    semantics::ModuleSemanticAnalyzer semantic_analyzer;
    result.semantic_result = semantic_analyzer.analyze(result.parse_result.module);
    if (result.semantic_result.has_errors()) {
        result.error_text = result.semantic_result.render(result.source_file->path().string());
    }
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
    return result;
}

auto CompilePipeline::emit_llvm(std::filesystem::path const& source_path) const -> CompilePipelineResult {
    return emit_llvm(source_path, CompilePipelineOptions {});
}

auto CompilePipeline::emit_llvm(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    auto result = analyze(source_path, options);
    if (result.has_errors()) {
        return result;
    }

    lowering::LlvmIrEmitter emitter;
    auto emission_options = build_lowering_emission_options(result, options, LoweringEmissionMode::full_ir);
    auto emission = emitter.emit(result.parse_result.module, result.semantic_result, emission_options);
    if (emission.has_errors()) {
        result.error_text = emission.render(result.source_file->path().string());
        return result;
    }
    copy_lowering_emission_reports(result, std::move(emission), options);
    return result;
}

DynamicArrayCleanupMetadataCollector::DynamicArrayCleanupMetadataCollector(CompilePipeline const& pipeline)
    : pipeline_(pipeline) {}

auto DynamicArrayCleanupMetadataCollector::collect(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    auto result = pipeline_.analyze(source_path, options);
    if (result.has_errors()) {
        return result;
    }

    lowering::LlvmIrEmitter emitter;
    auto emission_options =
        build_lowering_emission_options(result, options, LoweringEmissionMode::dynamic_array_cleanup_metadata);
    auto emission = emitter.emit_metadata(result.parse_result.module, result.semantic_result, emission_options);
    if (emission.has_errors()) {
        result.error_text = emission.render(result.source_file->path().string());
        return result;
    }
    copy_lowering_emission_reports(result, std::move(emission), options);
    return result;
}

auto CompilePipeline::collect_dynamic_array_cleanup_metadata(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    return DynamicArrayCleanupMetadataCollector(*this).collect(source_path, options);
}

auto CompilePipeline::emit_object(std::filesystem::path const& source_path) const -> CompilePipelineResult {
    return emit_object(source_path, CompilePipelineOptions {});
}

auto CompilePipeline::emit_object(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) const -> CompilePipelineResult {
    auto result = emit_llvm(source_path, options);
    if (result.has_errors()) {
        return result;
    }

    lowering::LlvmObjectEmitter emitter;
    auto emission = emitter.emit(result.ir_text);
    if (emission.has_errors()) {
        result.error_text = emission.diagnostics.render(result.source_file->path().string());
        return result;
    }
    result.object_bytes = std::move(emission.object_bytes);
    return result;
}

}  // namespace orison::pipeline
