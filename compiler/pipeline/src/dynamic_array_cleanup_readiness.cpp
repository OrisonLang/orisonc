#include "dynamic_array_cleanup_readiness.hpp"

#include "lowering_emission_options.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>

namespace orison::pipeline {
namespace {

auto report_contains(std::vector<std::string> const& lines, std::string_view fragment) -> bool {
    return std::ranges::any_of(lines, [&](auto const& line) {
        return line.find(fragment) != std::string::npos;
    });
}

}  // namespace

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

}  // namespace orison::pipeline
