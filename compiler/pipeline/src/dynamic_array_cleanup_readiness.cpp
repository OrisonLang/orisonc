#include "dynamic_array_cleanup_readiness.hpp"

#include "lowering_emission_options.hpp"

#include <sstream>

namespace orison::pipeline {
namespace {

auto format_dynamic_array_descriptor_binding_kind(
    semantics::DynamicArrayDescriptorBindingKind binding_kind
) -> std::string_view {
    switch (binding_kind) {
        case semantics::DynamicArrayDescriptorBindingKind::local_binding:
            return "local";
        case semantics::DynamicArrayDescriptorBindingKind::parameter_binding:
            return "parameter";
        case semantics::DynamicArrayDescriptorBindingKind::returned_binding:
            return "returned";
    }
    return "unknown";
}

}  // namespace

auto plan_dynamic_array_cleanup_production_readiness(
    CompilePipelineResult const& result,
    CompilePipelineOptions const& options
) -> DynamicArrayCleanupProductionReadiness {
    auto const& availability = result.dynamic_array_cleanup_availability;
    return DynamicArrayCleanupProductionReadiness {
        .missing_element_drop_pairs = availability.missing_element_drop_pairs,
        .descriptor_summary_blockers = result.dynamic_array_descriptor_lifetime_plan_state.summary_blockers,
        .descriptor_summaries_available = availability.descriptor_summaries_available,
        .descriptor_summary_blockers_absent = availability.descriptor_summary_blockers_absent,
        .descriptor_cleanup_plans_available = availability.descriptor_cleanup_plans_available,
        .cleanup_obligations_available = availability.cleanup_obligations_available,
        .sequence_verification_available = availability.sequence_verification_available,
        .sequence_verification_passed = availability.sequence_verification_passed,
        .cleanup_capability_proven = availability.cleanup_capability_proven,
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
    return readiness.descriptor_summaries_available &&
        readiness.descriptor_summary_blockers_absent &&
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
    auto const blocker_status = [](bool blockers_absent) {
        return blockers_absent ? "absent" : "present";
    };
    auto output = std::ostringstream {};
    output << "dynamic array cleanup production readiness ";
    output << (dynamic_array_cleanup_production_ready(readiness) ? "ready" : "blocked");
    output << " [descriptor origins " << status(readiness.descriptor_summaries_available) << "]";
    output << " [descriptor origin blockers " << blocker_status(readiness.descriptor_summary_blockers_absent) << "]";
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

auto format_dynamic_array_cleanup_production_readiness_diagnostics(
    DynamicArrayCleanupProductionReadiness const& readiness
) -> std::vector<std::string> {
    auto diagnostics = std::vector<std::string> {};
    diagnostics.reserve(readiness.descriptor_summary_blockers.size());
    for (auto const& blocker : readiness.descriptor_summary_blockers) {
        auto output = std::ostringstream {};
        output << "dynamic array cleanup production blocked: descriptor lifetime metadata "
               << blocker.reason
               << " owner " << blocker.owner_name
               << " source " << blocker.source_type_name
               << " element " << blocker.element_source_type_name
               << " origin " << format_dynamic_array_descriptor_binding_kind(blocker.binding_kind);
        if (blocker.source_line != 0) {
            output << " line " << blocker.source_line;
        }
        diagnostics.push_back(output.str());
    }
    return diagnostics;
}

}  // namespace orison::pipeline
