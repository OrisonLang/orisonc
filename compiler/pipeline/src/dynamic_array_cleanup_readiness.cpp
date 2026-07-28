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

auto collect_bracketed_values_after_marker(
    std::vector<std::string> const& lines,
    std::string_view marker
) -> std::vector<std::string> {
    auto values = std::vector<std::string> {};
    for (auto const& line : lines) {
        auto marker_position = line.find(marker);
        if (marker_position == std::string::npos) {
            continue;
        }
        auto cursor = marker_position + marker.size();
        while (cursor < line.size()) {
            auto open = line.find('[', cursor);
            if (open == std::string::npos) {
                break;
            }
            auto close = line.find(']', open + 1);
            if (close == std::string::npos) {
                break;
            }
            auto value = line.substr(open + 1, close - open - 1);
            if (value.find(' ') != std::string::npos) {
                break;
            }
            values.push_back(value);
            cursor = close + 1;
        }
    }
    return values;
}

}  // namespace

auto plan_dynamic_array_cleanup_production_readiness(
    CompilePipelineResult const& result,
    CompilePipelineOptions const& options
) -> DynamicArrayCleanupProductionReadiness {
    return DynamicArrayCleanupProductionReadiness {
        .missing_element_drop_pairs = collect_bracketed_values_after_marker(
            result.dynamic_array_cleanup_emission_capability_report,
            "missing-element-drop-pairs"
        ),
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
