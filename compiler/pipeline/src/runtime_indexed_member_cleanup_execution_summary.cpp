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

    auto report = std::ostringstream {};
    report << "runtime-index member cleanup execution-summary"
           << " owner " << gate.owner_name
           << " index " << gate.index_expression_text
           << " element " << gate.element_source_type_name
           << " moved " << gate.moved_source_type_name
           << " member-path " << dotted_path(gate.moved_member_path)
           << " typed-gate " << (gate.gate_ready ? "ready" : "blocked")
           << " apply "
           << (apply_authorization != nullptr && apply_authorization->apply_authorized ? "authorized" : "blocked")
           << " rewrite-authorization "
           << (rewrite_authorization != nullptr && rewrite_authorization->rewrite_authorized ? "authorized" : "blocked")
           << " rewrite-execution "
           << (execution_plan != nullptr && execution_plan->execution_enabled ? "enabled" : "blocked")
           << " rewrite-verdict "
           << (execution_verdict != nullptr && execution_verdict->execution_enabled ? "enabled" : "blocked")
           << " rewrite-promotion "
           << (promotion_status != nullptr && promotion_status->promotion_ready ? "ready" : "blocked")
           << " helper-bindings " << result.runtime_indexed_member_cleanup_helper_drop_bindings.size()
           << " helper-target "
           << (
               helper_bindings != nullptr && !helper_bindings->helper_symbol_name.empty()
                   ? helper_bindings->helper_symbol_name
                   : "missing"
           )
           << " helper-sibling-bindings "
           << (helper_bindings != nullptr ? helper_bindings->sibling_binding_count : std::size_t {0})
           << " helper-definition "
           << (helper_bindings != nullptr && helper_bindings->helper_definition_ready ? "ready" : "blocked")
           << " production " << (gate.production_enabled ? "enabled" : "disabled");
    return {report.str()};
}

}  // namespace orison::pipeline
