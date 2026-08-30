#include "orison/pipeline/runtime_indexed_member_cleanup_readiness_report.hpp"

#include "orison/lowering/ownership_transfer.hpp"
#include "orison/pipeline/runtime_indexed_member_cleanup_match_key.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <sstream>
#include <string>

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

auto runtime_indexed_member_cleanup_mutation_line(std::string const& line) -> bool {
    auto constexpr prefix = std::string_view {"runtime-index member cleanup mutation"};
    return line.starts_with(prefix);
}

auto runtime_indexed_member_cleanup_production_blocker_line(std::string const& line) -> bool {
    auto constexpr prefix = std::string_view {"runtime-index member cleanup production blocker"};
    return line.starts_with(prefix);
}

auto runtime_indexed_member_cleanup_should_include_source_text(std::string const& line) -> bool {
    return runtime_indexed_member_cleanup_mutation_line(line) ||
        runtime_indexed_member_cleanup_production_blocker_line(line);
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

auto enrich_runtime_indexed_member_cleanup_source_text_line(
    std::string line,
    std::string const& source_text
) -> std::string {
    if (!runtime_indexed_member_cleanup_should_include_source_text(line) ||
        line.find(" source-text ") != std::string::npos) {
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

auto enrich_runtime_indexed_member_cleanup_source_text_lines(
    std::vector<std::string> lines,
    CompilePipelineResult const& result
) -> std::vector<std::string> {
    auto const source_text = result.source_file ? result.source_file->content() : std::string {};
    if (source_text.empty()) {
        return lines;
    }
    for (auto& line : lines) {
        line = enrich_runtime_indexed_member_cleanup_source_text_line(std::move(line), source_text);
    }
    return lines;
}

auto has_only_stale_production_readiness_blockers(
    lowering::RuntimeIndexedMemberCleanupProductionReadiness const& readiness
) -> bool;

auto production_readiness_satisfied_for_promotion(
    lowering::RuntimeIndexedMemberCleanupTypedPromotionGate const& gate,
    lowering::RuntimeIndexedMemberCleanupProductionReadiness const& readiness
) -> bool;

auto runtime_indexed_member_cleanup_promoted(
    CompilePipelineResult const& result,
    RuntimeIndexedMemberCleanupMatchKey const& key
) -> bool;

void append_keyed_member_cleanup_chain(
    std::vector<std::string>& lines,
    CompilePipelineResult const& result,
    RuntimeIndexedMemberCleanupMatchKey const& key
) {
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_helper_drop_bindings,
        lowering::runtime_indexed_member_cleanup_helper_drop_bindings_report
    );
    if (!runtime_indexed_member_cleanup_promoted(result, key)) {
        append_runtime_indexed_member_cleanup_record_line_with_diagnostics(
            lines,
            key,
            result.runtime_indexed_member_cleanup_production_readiness,
            lowering::runtime_indexed_member_cleanup_production_readiness_report,
            lowering::runtime_indexed_member_cleanup_production_blocker_diagnostics
        );
    }
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_operation_plans,
        lowering::runtime_indexed_member_cleanup_mutation_operation_plan_report
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_operation_validations,
        lowering::runtime_indexed_member_cleanup_mutation_operation_validation_report
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_conflict_detections,
        lowering::runtime_indexed_member_cleanup_mutation_conflict_detection_report
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_apply_authorizations,
        lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_apply_previews,
        lowering::runtime_indexed_member_cleanup_mutation_apply_preview_report
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_post_apply_verifications,
        lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification_report
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_promotion_summaries,
        lowering::runtime_indexed_member_cleanup_mutation_promotion_summary_report
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_typed_promotion_gates,
        lowering::runtime_indexed_member_cleanup_typed_promotion_gate_report
    );
    append_runtime_indexed_member_cleanup_record_line_with_diagnostics(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_production_readiness,
        lowering::runtime_indexed_member_cleanup_mutation_production_readiness_report,
        lowering::runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_readiness_verdicts,
        lowering::runtime_indexed_member_cleanup_mutation_readiness_verdict_report
    );
    append_runtime_indexed_member_cleanup_record_line_with_diagnostics(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_rewrite_authorizations,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_report,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_diagnostics
    );
    append_runtime_indexed_member_cleanup_record_line_with_diagnostics(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report
    );
    append_runtime_indexed_member_cleanup_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_promotion_status_report
    );
}

void append_ungated_member_cleanup_lines(
    std::vector<std::string>& lines,
    CompilePipelineResult const& result
) {
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_helper_drop_bindings,
        lowering::runtime_indexed_member_cleanup_helper_drop_bindings_report
    );
    append_runtime_indexed_member_cleanup_record_lines_with_diagnostics(
        lines,
        result.runtime_indexed_member_cleanup_production_readiness,
        lowering::runtime_indexed_member_cleanup_production_readiness_report,
        lowering::runtime_indexed_member_cleanup_production_blocker_diagnostics
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_operation_plans,
        lowering::runtime_indexed_member_cleanup_mutation_operation_plan_report
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_operation_validations,
        lowering::runtime_indexed_member_cleanup_mutation_operation_validation_report
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_conflict_detections,
        lowering::runtime_indexed_member_cleanup_mutation_conflict_detection_report
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_apply_authorizations,
        lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_apply_previews,
        lowering::runtime_indexed_member_cleanup_mutation_apply_preview_report
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_post_apply_verifications,
        lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification_report
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_promotion_summaries,
        lowering::runtime_indexed_member_cleanup_mutation_promotion_summary_report
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_typed_promotion_gates,
        lowering::runtime_indexed_member_cleanup_typed_promotion_gate_report
    );
    append_runtime_indexed_member_cleanup_record_lines_with_diagnostics(
        lines,
        result.runtime_indexed_member_cleanup_mutation_production_readiness,
        lowering::runtime_indexed_member_cleanup_mutation_production_readiness_report,
        lowering::runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_readiness_verdicts,
        lowering::runtime_indexed_member_cleanup_mutation_readiness_verdict_report
    );
    append_runtime_indexed_member_cleanup_record_lines_with_diagnostics(
        lines,
        result.runtime_indexed_member_cleanup_mutation_rewrite_authorizations,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_report,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_diagnostics
    );
    append_runtime_indexed_member_cleanup_record_lines_with_diagnostics(
        lines,
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report
    );
    append_runtime_indexed_member_cleanup_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_promotion_status_report
    );
}

void append_promotion_blocker_line(
    std::vector<std::string>& lines,
    lowering::RuntimeIndexedMemberCleanupTypedPromotionGate const& gate,
    std::string const& blocker,
    std::string const& detail
) {
    auto line = std::ostringstream {};
    line << "runtime-index member cleanup promotion blocker"
         << " owner " << gate.owner_name
         << " index " << gate.index_expression_text
         << " element " << gate.element_source_type_name
         << " moved " << gate.moved_source_type_name
         << " member-path " << runtime_indexed_member_cleanup_dotted_path(gate.moved_member_path)
         << " blocker " << blocker
         << " detail " << detail;
    lines.push_back(line.str());
}

auto has_only_stale_production_readiness_blockers(
    lowering::RuntimeIndexedMemberCleanupProductionReadiness const& readiness
) -> bool {
    for (auto const& blocker : readiness.blockers) {
        if (blocker != "member-cleanup-module-mutation" &&
            blocker != "production-member-cleanup") {
            return false;
        }
    }
    return true;
}

auto production_readiness_satisfied_for_promotion(
    lowering::RuntimeIndexedMemberCleanupTypedPromotionGate const& gate,
    lowering::RuntimeIndexedMemberCleanupProductionReadiness const& readiness
) -> bool {
    if (readiness.production_ready) {
        return true;
    }
    return gate.production_enabled &&
        readiness.proof_ready &&
        readiness.target_metadata_ready &&
        readiness.helper_drop_bindings_ready &&
        readiness.cfg_slice_ready &&
        has_only_stale_production_readiness_blockers(readiness);
}

auto runtime_indexed_member_cleanup_promoted(
    CompilePipelineResult const& result,
    RuntimeIndexedMemberCleanupMatchKey const& key
) -> bool {
    auto const* gate = find_runtime_indexed_member_cleanup_record(
        key,
        result.runtime_indexed_member_cleanup_typed_promotion_gates
    );
    auto const* production_readiness = find_runtime_indexed_member_cleanup_record(
        key,
        result.runtime_indexed_member_cleanup_production_readiness
    );
    auto const* mutation_readiness = find_runtime_indexed_member_cleanup_record(
        key,
        result.runtime_indexed_member_cleanup_mutation_production_readiness
    );
    auto const* rewrite_promotion = find_runtime_indexed_member_cleanup_record(
        key,
        result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses
    );
    return gate != nullptr &&
        production_readiness != nullptr &&
        mutation_readiness != nullptr &&
        rewrite_promotion != nullptr &&
        production_readiness_satisfied_for_promotion(*gate, *production_readiness) &&
        result.runtime_indexed_cleanup_module_ir_production_readiness_state.ir_shape_ready &&
        gate->production_enabled &&
        mutation_readiness->production_enabled &&
        rewrite_promotion->production_enabled;
}

auto runtime_indexed_cleanup_module_ir_shape_blocker_detail(
    CompilePipelineResult const& result
) -> std::string {
    for (auto const& plan : result.runtime_indexed_cleanup_emission_plan_state.plans) {
        if (plan.gated_ir_slice_lines.empty()) {
            continue;
        }
        auto const shape = runtime_indexed_cleanup_ir_shape_summary(plan);
        auto const shape_ready =
            shape.common_loop_shape_ready &&
            shape.drop_call_found &&
            (shape.descriptor_storage_shape_ready || shape.inline_storage_shape_ready);
        if (shape_ready) {
            continue;
        }
        auto detail = std::ostringstream {};
        detail << "runtime-index cleanup module-ir shape is blocked"
               << " owner " << shape.owner_name
               << " common-loop " << (shape.common_loop_shape_ready ? "ready" : "blocked")
               << " drop-call " << (shape.drop_call_found ? "ready" : "blocked")
               << " descriptor-storage " << (shape.descriptor_storage_shape_ready ? "ready" : "blocked")
               << " inline-storage " << (shape.inline_storage_shape_ready ? "ready" : "blocked")
               << " descriptor-load " << (shape.descriptor_load_found ? "present" : "absent")
               << " descriptor-gep " << (shape.descriptor_element_gep_found ? "present" : "absent")
               << " inline-gep " << (shape.inline_element_gep_found ? "present" : "absent")
               << " zero-store " << (shape.zero_store_found ? "present" : "absent")
               << " deallocate " << (shape.deallocate_call_found ? "present" : "absent");
        return detail.str();
    }
    return "runtime-index cleanup module-ir shape is blocked";
}

}  // namespace

auto runtime_indexed_member_cleanup_promotion_state(
    CompilePipelineResult const& result
) -> RuntimeIndexedMemberCleanupPromotionState {
    auto const module_ir_shape_blocker_detail =
        runtime_indexed_cleanup_module_ir_shape_blocker_detail(result);
    auto state = RuntimeIndexedMemberCleanupPromotionState {
        .production_readiness_count = result.runtime_indexed_member_cleanup_production_readiness.size(),
        .typed_gate_count = result.runtime_indexed_member_cleanup_typed_promotion_gates.size(),
        .mutation_readiness_count = result.runtime_indexed_member_cleanup_mutation_production_readiness.size(),
        .rewrite_promotion_count = result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.size(),
        .module_ir_shape_ready = result.runtime_indexed_cleanup_module_ir_production_readiness_state.ir_shape_ready,
        .module_ir_shape_blocker_detail =
            result.runtime_indexed_cleanup_module_ir_production_readiness_state.ir_shape_ready
                ? std::string {}
                : module_ir_shape_blocker_detail,
    };
    auto const has_member_cleanup_records =
        state.production_readiness_count > 0 ||
        state.typed_gate_count > 0 ||
        state.mutation_readiness_count > 0 ||
        state.rewrite_promotion_count > 0;
    if (!has_member_cleanup_records) {
        return state;
    }

    if (result.runtime_indexed_member_cleanup_typed_promotion_gates.empty()) {
        state.state = "blocked";
        return state;
    }

    auto ready = true;
    for (auto const& gate : result.runtime_indexed_member_cleanup_typed_promotion_gates) {
        auto const key = runtime_indexed_member_cleanup_match_key(gate);
        auto const* production_readiness = find_runtime_indexed_member_cleanup_record(
            key,
            result.runtime_indexed_member_cleanup_production_readiness
        );
        auto const* mutation_readiness = find_runtime_indexed_member_cleanup_record(
            key,
            result.runtime_indexed_member_cleanup_mutation_production_readiness
        );
        auto const* rewrite_promotion = find_runtime_indexed_member_cleanup_record(
            key,
            result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses
        );
        ready =
            ready &&
            production_readiness != nullptr &&
            mutation_readiness != nullptr &&
            rewrite_promotion != nullptr &&
            production_readiness_satisfied_for_promotion(gate, *production_readiness) &&
            state.module_ir_shape_ready &&
            gate.production_enabled &&
            mutation_readiness->production_enabled &&
            rewrite_promotion->production_enabled;
    }
    state.state = ready ? "ready" : "blocked";
    return state;
}

auto runtime_indexed_member_cleanup_promotion_state_report_lines(
    CompilePipelineResult const& result
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    if (result.runtime_indexed_member_cleanup_typed_promotion_gates.empty()) {
        return lines;
    }

    auto const module_ir_shape_blocker_detail =
        runtime_indexed_cleanup_module_ir_shape_blocker_detail(result);
    for (auto const& gate : result.runtime_indexed_member_cleanup_typed_promotion_gates) {
        auto const key = runtime_indexed_member_cleanup_match_key(gate);
        auto const* production_readiness = find_runtime_indexed_member_cleanup_record(
            key,
            result.runtime_indexed_member_cleanup_production_readiness
        );
        auto const* mutation_readiness = find_runtime_indexed_member_cleanup_record(
            key,
            result.runtime_indexed_member_cleanup_mutation_production_readiness
        );
        auto const* rewrite_promotion = find_runtime_indexed_member_cleanup_record(
            key,
            result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses
        );

        if (production_readiness == nullptr) {
            append_promotion_blocker_line(
                lines,
                gate,
                "missing-production-readiness",
                "matching member cleanup production-readiness record is missing"
            );
        } else if (!production_readiness_satisfied_for_promotion(gate, *production_readiness)) {
            append_promotion_blocker_line(
                lines,
                gate,
                "blocked-production-readiness",
                "matching member cleanup production-readiness record is blocked"
            );
        }
        if (!result.runtime_indexed_cleanup_module_ir_production_readiness_state.ir_shape_ready) {
            append_promotion_blocker_line(
                lines,
                gate,
                "blocked-module-ir-shape",
                module_ir_shape_blocker_detail
            );
        }
        if (!gate.production_enabled) {
            append_promotion_blocker_line(
                lines,
                gate,
                "typed-promotion-disabled",
                "typed promotion gate production is disabled"
            );
        }
        if (mutation_readiness == nullptr) {
            append_promotion_blocker_line(
                lines,
                gate,
                "missing-mutation-readiness",
                "matching member cleanup mutation-readiness record is missing"
            );
        } else if (!mutation_readiness->production_enabled) {
            append_promotion_blocker_line(
                lines,
                gate,
                "blocked-mutation-readiness",
                "matching member cleanup mutation-readiness record production is disabled"
            );
        }
        if (rewrite_promotion == nullptr) {
            append_promotion_blocker_line(
                lines,
                gate,
                "missing-rewrite-promotion",
                "matching member cleanup rewrite-promotion record is missing"
            );
        } else if (!rewrite_promotion->production_enabled) {
            append_promotion_blocker_line(
                lines,
                gate,
                "blocked-rewrite-promotion",
                "matching member cleanup rewrite-promotion record production is disabled"
            );
        }
    }
    return lines;
}

auto runtime_indexed_member_cleanup_readiness_report_lines(
    CompilePipelineResult const& result
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    if (result.runtime_indexed_member_cleanup_typed_promotion_gates.empty()) {
        append_ungated_member_cleanup_lines(lines, result);
        return enrich_runtime_indexed_member_cleanup_source_text_lines(std::move(lines), result);
    }

    for (auto const& gate : result.runtime_indexed_member_cleanup_typed_promotion_gates) {
        append_keyed_member_cleanup_chain(
            lines,
            result,
            runtime_indexed_member_cleanup_match_key(gate)
        );
    }
    return enrich_runtime_indexed_member_cleanup_source_text_lines(std::move(lines), result);
}

}  // namespace orison::pipeline
