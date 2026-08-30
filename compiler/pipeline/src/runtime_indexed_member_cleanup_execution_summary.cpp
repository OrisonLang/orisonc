#include "orison/pipeline/runtime_indexed_member_cleanup_execution_summary.hpp"

#include "orison/pipeline/runtime_indexed_member_cleanup_match_key.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>

namespace orison::pipeline {
namespace {

auto trim_source_line_text(std::string line) -> std::string {
    auto const first_non_space = std::find_if(
        line.begin(),
        line.end(),
        [](unsigned char character) {
            return !std::isspace(character);
        }
    );
    if (first_non_space == line.end()) {
        return {};
    }
    auto const last_non_space = std::find_if(
        line.rbegin(),
        line.rend(),
        [](unsigned char character) {
            return !std::isspace(character);
        }
    ).base();
    return std::string(first_non_space, last_non_space);
}

auto source_line_text(std::string const& source_text, std::size_t line_number) -> std::string {
    if (line_number == 0) {
        return {};
    }
    auto current_line = std::size_t {1};
    auto line_start = std::size_t {0};
    while (line_start <= source_text.size()) {
        auto line_end = source_text.find('\n', line_start);
        if (line_end == std::string::npos) {
            line_end = source_text.size();
        }
        if (current_line == line_number) {
            return trim_source_line_text(source_text.substr(line_start, line_end - line_start));
        }
        if (line_end == source_text.size()) {
            break;
        }
        line_start = line_end + 1;
        ++current_line;
    }
    return {};
}

auto source_line_token_value(std::string const& line) -> std::optional<std::pair<std::size_t, std::size_t>> {
    auto constexpr token = std::string_view {" source-line "};
    auto const token_start = line.find(token);
    if (token_start == std::string::npos) {
        return std::nullopt;
    }

    auto const digits_start = token_start + token.size();
    auto digits_end = digits_start;
    auto line_number = std::size_t {0};
    while (digits_end < line.size() && std::isdigit(static_cast<unsigned char>(line[digits_end]))) {
        line_number = (line_number * 10) + static_cast<std::size_t>(line[digits_end] - '0');
        ++digits_end;
    }
    if (digits_end == digits_start || line_number == 0) {
        return std::nullopt;
    }
    return std::pair {line_number, digits_end};
}

auto enrich_source_text(std::string line, std::string const& source_text) -> std::string {
    if (source_text.empty() || line.find(" source-text ") != std::string::npos) {
        return line;
    }
    auto const source_line = source_line_token_value(line);
    if (!source_line.has_value()) {
        return line;
    }
    auto const source_snippet = source_line_text(source_text, source_line->first);
    if (source_snippet.empty()) {
        return line;
    }
    line.insert(source_line->second, " source-text " + source_snippet);
    return line;
}

void append_source_line(std::ostringstream& report, std::size_t source_line) {
    if (source_line != 0) {
        report << " source-line " << source_line;
    }
}

}  // namespace

auto runtime_indexed_member_cleanup_execution_summary_report_lines(
    CompilePipelineResult const& result
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    auto const source_text = result.source_file ? result.source_file->content() : std::string {};
    for (auto const& summary : result.runtime_indexed_member_cleanup_execution_summaries) {
        lines.push_back(enrich_source_text(
            runtime_indexed_member_cleanup_execution_summary_report(summary),
            source_text
        ));
    }
    return lines;
}

auto runtime_indexed_member_cleanup_execution_summaries(
    CompilePipelineResult const& result
) -> std::vector<RuntimeIndexedMemberCleanupExecutionSummary> {
    auto summaries = std::vector<RuntimeIndexedMemberCleanupExecutionSummary> {};
    summaries.reserve(result.runtime_indexed_member_cleanup_typed_promotion_gates.size());
    for (auto const& gate : result.runtime_indexed_member_cleanup_typed_promotion_gates) {
        auto const* apply_authorization = find_runtime_indexed_member_cleanup_record(
            gate,
            result.runtime_indexed_member_cleanup_mutation_apply_authorizations
        );
        auto const* rewrite_authorization = find_runtime_indexed_member_cleanup_record(
            gate,
            result.runtime_indexed_member_cleanup_mutation_rewrite_authorizations
        );
        auto const* execution_plan = find_runtime_indexed_member_cleanup_record(
            gate,
            result.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans
        );
        auto const* execution_verdict = find_runtime_indexed_member_cleanup_record(
            gate,
            result.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts
        );
        auto const* promotion_status = find_runtime_indexed_member_cleanup_record(
            gate,
            result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses
        );
        auto const* helper_bindings = find_runtime_indexed_member_cleanup_record(
            gate,
            result.runtime_indexed_member_cleanup_helper_drop_bindings
        );
        summaries.push_back(RuntimeIndexedMemberCleanupExecutionSummary {
            .owner_name = gate.owner_name,
            .index_expression_text = gate.index_expression_text,
            .element_source_type_name = gate.element_source_type_name,
            .moved_source_type_name = gate.moved_source_type_name,
            .moved_member_path = gate.moved_member_path,
            .source_line = gate.source_line,
            .helper_symbol_name = helper_bindings != nullptr ? helper_bindings->helper_symbol_name : std::string {},
            .helper_binding_count = count_runtime_indexed_member_cleanup_records(
                gate,
                result.runtime_indexed_member_cleanup_helper_drop_bindings
            ),
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
        });
    }
    return summaries;
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
           << " member-path " << runtime_indexed_member_cleanup_dotted_path(summary.moved_member_path);
    append_source_line(report, summary.source_line);
    report << " typed-gate " << (summary.typed_gate_ready ? "ready" : "blocked")
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
