#include "dynamic_array_cleanup_readiness.hpp"

#include "lowering_emission_options.hpp"

#include <sstream>

namespace orison::pipeline {

auto plan_dynamic_array_cleanup_production_readiness(
    CompilePipelineResult const& result,
    CompilePipelineOptions const& options
) -> DynamicArrayCleanupProductionReadiness {
    return DynamicArrayCleanupProductionReadiness {
        .missing_element_drop_pairs = result.dynamic_array_cleanup_missing_element_drop_pairs,
        .descriptor_origins_available = !result.semantic_dynamic_array_descriptor_origin_report.empty(),
        .descriptor_cleanup_plans_available = !result.dynamic_array_descriptor_cleanup_plan_report.empty(),
        .cleanup_obligations_available = !result.dynamic_array_cleanup_obligation_report.empty(),
        .sequence_verification_available = !result.dynamic_array_cleanup_sequence_verification_report.empty(),
        .sequence_verification_passed = result.dynamic_array_cleanup_sequence_verification_passed,
        .cleanup_capability_proven = result.dynamic_array_cleanup_capability_proven,
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
    if (!readiness.missing_element_drop_pairs.empty()) {
        output << " missing-element-drop-pairs";
        for (auto const& missing_element_drop_pair : readiness.missing_element_drop_pairs) {
            output << " [" << missing_element_drop_pair << "]";
        }
    }
    output << " [production signatures " << status(readiness.production_signature_lowering_enabled) << "]";
    output << " [production construction " << status(readiness.production_construction_lowering_enabled) << "]";
    output << " [production cleanup emission " << status(readiness.production_cleanup_emission_enabled) << "]";
    output << " (metadata only)";
    return output.str();
}

}  // namespace orison::pipeline
