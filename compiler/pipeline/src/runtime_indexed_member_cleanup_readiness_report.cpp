#include "orison/pipeline/runtime_indexed_member_cleanup_readiness_report.hpp"

#include "orison/lowering/ownership_transfer.hpp"
#include "orison/pipeline/runtime_indexed_member_cleanup_match_key.hpp"

#include <algorithm>

namespace orison::pipeline {
namespace {

template <typename Record>
auto find_member_cleanup_record_by_key(
    RuntimeIndexedMemberCleanupMatchKey const& key,
    std::vector<Record> const& records
) -> Record const* {
    auto const found = std::find_if(
        records.begin(),
        records.end(),
        [&](Record const& record) {
            return runtime_indexed_member_cleanup_match_key(record) == key;
        }
    );
    if (found == records.end()) {
        return nullptr;
    }
    return &*found;
}

template <typename Record, typename Render>
void append_keyed_record_line(
    std::vector<std::string>& lines,
    RuntimeIndexedMemberCleanupMatchKey const& key,
    std::vector<Record> const& records,
    Render render
) {
    if (auto const* record = find_member_cleanup_record_by_key(key, records)) {
        lines.push_back(render(*record));
    }
}

template <typename Record, typename Render, typename Diagnostics>
void append_keyed_record_line_with_diagnostics(
    std::vector<std::string>& lines,
    RuntimeIndexedMemberCleanupMatchKey const& key,
    std::vector<Record> const& records,
    Render render,
    Diagnostics diagnostics
) {
    if (auto const* record = find_member_cleanup_record_by_key(key, records)) {
        lines.push_back(render(*record));
        auto diagnostic_lines = diagnostics(*record);
        lines.insert(lines.end(), diagnostic_lines.begin(), diagnostic_lines.end());
    }
}

template <typename Record, typename Render>
void append_all_record_lines(
    std::vector<std::string>& lines,
    std::vector<Record> const& records,
    Render render
) {
    for (auto const& record : records) {
        lines.push_back(render(record));
    }
}

template <typename Record, typename Render, typename Diagnostics>
void append_all_record_lines_with_diagnostics(
    std::vector<std::string>& lines,
    std::vector<Record> const& records,
    Render render,
    Diagnostics diagnostics
) {
    for (auto const& record : records) {
        lines.push_back(render(record));
        auto diagnostic_lines = diagnostics(record);
        lines.insert(lines.end(), diagnostic_lines.begin(), diagnostic_lines.end());
    }
}

void append_keyed_member_cleanup_chain(
    std::vector<std::string>& lines,
    CompilePipelineResult const& result,
    RuntimeIndexedMemberCleanupMatchKey const& key
) {
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_helper_drop_bindings,
        lowering::runtime_indexed_member_cleanup_helper_drop_bindings_report
    );
    append_keyed_record_line_with_diagnostics(
        lines,
        key,
        result.runtime_indexed_member_cleanup_production_readiness,
        lowering::runtime_indexed_member_cleanup_production_readiness_report,
        lowering::runtime_indexed_member_cleanup_production_blocker_diagnostics
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_operation_plans,
        lowering::runtime_indexed_member_cleanup_mutation_operation_plan_report
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_operation_validations,
        lowering::runtime_indexed_member_cleanup_mutation_operation_validation_report
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_conflict_detections,
        lowering::runtime_indexed_member_cleanup_mutation_conflict_detection_report
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_apply_authorizations,
        lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_apply_previews,
        lowering::runtime_indexed_member_cleanup_mutation_apply_preview_report
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_post_apply_verifications,
        lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification_report
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_promotion_summaries,
        lowering::runtime_indexed_member_cleanup_mutation_promotion_summary_report
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_typed_promotion_gates,
        lowering::runtime_indexed_member_cleanup_typed_promotion_gate_report
    );
    append_keyed_record_line_with_diagnostics(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_production_readiness,
        lowering::runtime_indexed_member_cleanup_mutation_production_readiness_report,
        lowering::runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_readiness_verdicts,
        lowering::runtime_indexed_member_cleanup_mutation_readiness_verdict_report
    );
    append_keyed_record_line_with_diagnostics(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_rewrite_authorizations,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_report,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_diagnostics
    );
    append_keyed_record_line_with_diagnostics(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics
    );
    append_keyed_record_line(
        lines,
        key,
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report
    );
    append_keyed_record_line(
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
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_helper_drop_bindings,
        lowering::runtime_indexed_member_cleanup_helper_drop_bindings_report
    );
    append_all_record_lines_with_diagnostics(
        lines,
        result.runtime_indexed_member_cleanup_production_readiness,
        lowering::runtime_indexed_member_cleanup_production_readiness_report,
        lowering::runtime_indexed_member_cleanup_production_blocker_diagnostics
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_operation_plans,
        lowering::runtime_indexed_member_cleanup_mutation_operation_plan_report
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_operation_validations,
        lowering::runtime_indexed_member_cleanup_mutation_operation_validation_report
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_conflict_detections,
        lowering::runtime_indexed_member_cleanup_mutation_conflict_detection_report
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_apply_authorizations,
        lowering::runtime_indexed_member_cleanup_mutation_apply_authorization_report
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_apply_previews,
        lowering::runtime_indexed_member_cleanup_mutation_apply_preview_report
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_post_apply_verifications,
        lowering::runtime_indexed_member_cleanup_mutation_post_apply_verification_report
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_promotion_summaries,
        lowering::runtime_indexed_member_cleanup_mutation_promotion_summary_report
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_typed_promotion_gates,
        lowering::runtime_indexed_member_cleanup_typed_promotion_gate_report
    );
    append_all_record_lines_with_diagnostics(
        lines,
        result.runtime_indexed_member_cleanup_mutation_production_readiness,
        lowering::runtime_indexed_member_cleanup_mutation_production_readiness_report,
        lowering::runtime_indexed_member_cleanup_mutation_production_readiness_diagnostics
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_readiness_verdicts,
        lowering::runtime_indexed_member_cleanup_mutation_readiness_verdict_report
    );
    append_all_record_lines_with_diagnostics(
        lines,
        result.runtime_indexed_member_cleanup_mutation_rewrite_authorizations,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_report,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_authorization_diagnostics
    );
    append_all_record_lines_with_diagnostics(
        lines,
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_plans,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_report,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_plan_diagnostics
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_rewrite_execution_verdicts,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_execution_verdict_report
    );
    append_all_record_lines(
        lines,
        result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses,
        lowering::runtime_indexed_member_cleanup_mutation_rewrite_promotion_status_report
    );
}

}  // namespace

auto runtime_indexed_member_cleanup_readiness_report_lines(
    CompilePipelineResult const& result
) -> std::vector<std::string> {
    auto lines = std::vector<std::string> {};
    if (result.runtime_indexed_member_cleanup_typed_promotion_gates.empty()) {
        append_ungated_member_cleanup_lines(lines, result);
        return lines;
    }

    for (auto const& gate : result.runtime_indexed_member_cleanup_typed_promotion_gates) {
        append_keyed_member_cleanup_chain(
            lines,
            result,
            runtime_indexed_member_cleanup_match_key(gate)
        );
    }
    return lines;
}

}  // namespace orison::pipeline
