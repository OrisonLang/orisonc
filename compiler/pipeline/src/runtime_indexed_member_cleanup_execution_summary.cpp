#include "orison/pipeline/runtime_indexed_member_cleanup_execution_summary.hpp"

#include <cstddef>
#include <sstream>

namespace orison::pipeline {
namespace {

auto dotted_path(std::vector<std::string> const& path) -> std::string {
    if (path.empty()) {
        return "none";
    }

    auto text = std::ostringstream {};
    for (auto index = std::size_t {0}; index < path.size(); ++index) {
        if (index > 0) {
            text << '.';
        }
        text << path[index];
    }
    return text.str();
}

}  // namespace

auto runtime_indexed_member_cleanup_execution_summary_report_lines(
    CompilePipelineResult const& result
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    for (auto const& summary : result.runtime_indexed_member_cleanup_execution_summaries) {
        lines.push_back(runtime_indexed_member_cleanup_execution_summary_report(summary));
    }
    return lines;
}

auto runtime_indexed_member_cleanup_execution_summaries(
    CompilePipelineResult const& result
) -> std::vector<RuntimeIndexedMemberCleanupExecutionSummary> {
    if (result.runtime_indexed_member_cleanup_typed_promotion_gates.empty()) {
        return {};
    }

    auto const& gate = result.runtime_indexed_member_cleanup_typed_promotion_gates.front();
    auto const* apply_authorization =
        result.runtime_indexed_member_cleanup_mutation_apply_authorizations.empty()
            ? nullptr
            : &result.runtime_indexed_member_cleanup_mutation_apply_authorizations.front();
    auto const* rewrite_authorization =
        result.runtime_indexed_member_cleanup_mutation_rewrite_authorizations.empty()
            ? nullptr
            : &result.runtime_indexed_member_cleanup_mutation_rewrite_authorizations.front();
    auto const* execution_plan =
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.empty()
            ? nullptr
            : &result.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans.front();
    auto const* execution_verdict =
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts.empty()
            ? nullptr
            : &result.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts.front();
    auto const* promotion_status =
        result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.empty()
            ? nullptr
            : &result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.front();
    auto const* helper_bindings =
        result.runtime_indexed_member_cleanup_helper_drop_bindings.empty()
            ? nullptr
            : &result.runtime_indexed_member_cleanup_helper_drop_bindings.front();

    return {RuntimeIndexedMemberCleanupExecutionSummary {
        .owner_name = gate.owner_name,
        .index_expression_text = gate.index_expression_text,
        .element_source_type_name = gate.element_source_type_name,
        .moved_source_type_name = gate.moved_source_type_name,
        .moved_member_path = gate.moved_member_path,
        .helper_symbol_name = helper_bindings != nullptr ? helper_bindings->helper_symbol_name : std::string {},
        .helper_binding_count = result.runtime_indexed_member_cleanup_helper_drop_bindings.size(),
        .helper_sibling_binding_count =
            helper_bindings != nullptr ? helper_bindings->sibling_binding_count : std::size_t {0},
        .typed_gate_ready = gate.gate_ready,
        .apply_authorized = apply_authorization != nullptr && apply_authorization->apply_authorized,
        .rewrite_authorized = rewrite_authorization != nullptr && rewrite_authorization->rewrite_authorized,
        .rewrite_execution_enabled = execution_plan != nullptr && execution_plan->execution_enabled,
        .rewrite_verdict_enabled = execution_verdict != nullptr && execution_verdict->execution_enabled,
        .rewrite_promotion_ready = promotion_status != nullptr && promotion_status->promotion_ready,
        .helper_definition_ready = helper_bindings != nullptr && helper_bindings->helper_definition_ready,
        .production_enabled = gate.production_enabled,
    }};
}

auto runtime_indexed_member_cleanup_execution_summary_report(
    RuntimeIndexedMemberCleanupExecutionSummary const& summary
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index member cleanup execution-summary"
           << " owner " << summary.owner_name
           << " index " << summary.index_expression_text
           << " element " << summary.element_source_type_name
           << " moved " << summary.moved_source_type_name
           << " member-path " << dotted_path(summary.moved_member_path)
           << " typed-gate " << (summary.typed_gate_ready ? "ready" : "blocked")
           << " apply " << (summary.apply_authorized ? "authorized" : "blocked")
           << " rewrite-authorization " << (summary.rewrite_authorized ? "authorized" : "blocked")
           << " rewrite-execution " << (summary.rewrite_execution_enabled ? "enabled" : "blocked")
           << " rewrite-verdict " << (summary.rewrite_verdict_enabled ? "enabled" : "blocked")
           << " rewrite-promotion " << (summary.rewrite_promotion_ready ? "ready" : "blocked")
           << " helper-bindings " << summary.helper_binding_count
           << " helper-target "
           << (summary.helper_symbol_name.empty() ? "missing" : summary.helper_symbol_name)
           << " helper-sibling-bindings " << summary.helper_sibling_binding_count
           << " helper-definition " << (summary.helper_definition_ready ? "ready" : "blocked")
           << " production " << (summary.production_enabled ? "enabled" : "disabled");
    return report.str();
}

}  // namespace orison::pipeline
