#include "orison/pipeline/runtime_indexed_member_cleanup_readiness_report.hpp"

#include "orison/lowering/ownership_transfer.hpp"
#include "orison/pipeline/runtime_indexed_member_cleanup_match_key.hpp"

#include <sstream>

namespace orison::pipeline {
namespace {

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
    append_runtime_indexed_member_cleanup_record_line_with_diagnostics(
        lines,
        key,
        result.runtime_indexed_member_cleanup_production_readiness,
        lowering::runtime_indexed_member_cleanup_production_readiness_report,
        lowering::runtime_indexed_member_cleanup_production_blocker_diagnostics
    );
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

}  // namespace

auto runtime_indexed_member_cleanup_promotion_state(
    CompilePipelineResult const& result
) -> RuntimeIndexedMemberCleanupPromotionState {
    auto state = RuntimeIndexedMemberCleanupPromotionState {
        .production_readiness_count = result.runtime_indexed_member_cleanup_production_readiness.size(),
        .typed_gate_count = result.runtime_indexed_member_cleanup_typed_promotion_gates.size(),
        .mutation_readiness_count = result.runtime_indexed_member_cleanup_mutation_production_readiness.size(),
        .rewrite_promotion_count = result.runtime_indexed_member_cleanup_mutation_rewrite_promotion_statuses.size(),
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
