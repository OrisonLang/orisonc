#include "lowering_emission_reports.hpp"

#include "orison/pipeline/drop_readiness_source_correlation_report.hpp"

#include "dynamic_array_cleanup_readiness.hpp"

#include <optional>
#include <sstream>
#include <string_view>
#include <utility>

namespace orison::pipeline {

namespace {

struct InsertedCleanupOperation {
    std::string operation_name;
    std::string source_owner_name;
    std::string target_owner_name;
};

auto parse_inserted_cleanup_operation(
    std::string_view line,
    std::string_view prefix
) -> std::optional<InsertedCleanupOperation> {
    if (!line.starts_with(prefix)) {
        return std::nullopt;
    }
    auto const payload = line.substr(prefix.size());
    auto const transfers_position = payload.find(" transfers ");
    if (transfers_position == std::string_view::npos) {
        return std::nullopt;
    }
    auto const source_start = transfers_position + std::string_view {" transfers "}.size();
    auto const target_separator = payload.find(" to ", source_start);
    if (target_separator == std::string_view::npos) {
        return std::nullopt;
    }
    auto const disabled_suffix = std::string_view {" (disabled)"};
    auto const suffix_position = payload.find(disabled_suffix, target_separator);
    if (suffix_position == std::string_view::npos) {
        return std::nullopt;
    }
    return InsertedCleanupOperation {
        .operation_name = std::string {payload.substr(0, transfers_position)},
        .source_owner_name = std::string {payload.substr(source_start, target_separator - source_start)},
        .target_owner_name = std::string {
            payload.substr(target_separator + std::string_view {" to "}.size(),
                           suffix_position - target_separator - std::string_view {" to "}.size())
        },
    };
}

auto format_inserted_cleanup_transition(
    InsertedCleanupOperation const& acquisition,
    InsertedCleanupOperation const& resumption
) -> std::string {
    auto output = std::ostringstream {};
    output << "computed DynamicArray for inserted cleanup transition";
    output << " acquire-from " << acquisition.source_owner_name;
    output << " acquire-to " << acquisition.target_owner_name;
    output << " acquire-operation " << acquisition.operation_name;
    output << " resume-from " << resumption.source_owner_name;
    output << " resume-to " << resumption.target_owner_name;
    output << " resume-operation " << resumption.operation_name;
    output << " (inserted IR)";
    return output.str();
}

auto format_inserted_cleanup_transition_report(std::string_view ir_text) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    auto pending_acquisition = std::optional<InsertedCleanupOperation> {};
    auto input = std::istringstream {std::string {ir_text}};
    auto line = std::string {};
    while (std::getline(input, line)) {
        if (auto acquisition = parse_inserted_cleanup_operation(
                line,
                "  ; cleanup acquisition operation "
            )) {
            pending_acquisition = std::move(acquisition);
            continue;
        }
        if (auto resumption = parse_inserted_cleanup_operation(
                line,
                "  ; cleanup resumption operation "
            )) {
            if (pending_acquisition.has_value() &&
                pending_acquisition->target_owner_name == resumption->source_owner_name &&
                pending_acquisition->source_owner_name == resumption->target_owner_name) {
                report.push_back(format_inserted_cleanup_transition(*pending_acquisition, *resumption));
            }
            pending_acquisition.reset();
        }
    }
    return report;
}

}  // namespace

void populate_lowering_emission_reports(
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
    result.computed_dynamic_array_for_descriptor_render_report =
        emission.computed_dynamic_array_for_descriptor_render_report();
    result.computed_dynamic_array_for_loop_control_render_report =
        emission.computed_dynamic_array_for_loop_control_render_report();
    result.computed_dynamic_array_for_element_address_render_report =
        emission.computed_dynamic_array_for_element_address_render_report();
    result.computed_dynamic_array_for_element_load_render_report =
        emission.computed_dynamic_array_for_element_load_render_report();
    result.computed_dynamic_array_for_loop_continue_render_report =
        emission.computed_dynamic_array_for_loop_continue_render_report();
    result.computed_dynamic_array_for_loop_render_sequence_report =
        emission.computed_dynamic_array_for_loop_render_sequence_report();
    result.computed_dynamic_array_for_loop_exit_cleanup_report =
        emission.computed_dynamic_array_for_loop_exit_cleanup_report();
    result.computed_dynamic_array_for_cleanup_transition_report =
        emission.computed_dynamic_array_for_cleanup_transition_report();
    result.computed_dynamic_array_for_inserted_cleanup_transition_report =
        format_inserted_cleanup_transition_report(result.ir_text);
    result.computed_dynamic_array_for_production_emission_gate_report =
        emission.computed_dynamic_array_for_production_emission_gate_report();
    result.computed_dynamic_array_for_production_sequence_report =
        emission.computed_dynamic_array_for_production_sequence_report();
    result.test_only_computed_dynamic_array_for_production_sequence_module_ir =
        std::move(emission.test_only_computed_dynamic_array_for_production_sequence_module_ir);
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

}  // namespace orison::pipeline
