#include "orison/driver/runtime_indexed_cleanup_reports.hpp"

#include "orison/pipeline/runtime_indexed_member_cleanup_execution_summary.hpp"
#include "orison/pipeline/runtime_indexed_member_cleanup_readiness_report.hpp"

#include <sstream>

namespace orison::driver {

auto runtime_indexed_constructor_move_production_readiness_report(
    pipeline::CompilePipelineResult const& result
) -> std::string {
    auto const has_constructor_move_gate_diagnostic =
        result.error_text.find("constructor-move disabled") != std::string::npos;
    auto const has_partial_ownership_diagnostic =
        result.error_text.find("indexed constructor ownership move requires explicit partial ownership support") !=
        std::string::npos;
    auto const member_cleanup_typed_promotion =
        pipeline::runtime_indexed_member_cleanup_promotion_state(result);
    auto const promoted_member_cleanup_ready = member_cleanup_typed_promotion.state == "ready";
    auto const cleanup_proof_ready =
        result.runtime_indexed_cleanup_capability_state.all_prerequisites_ready || promoted_member_cleanup_ready;
    auto const cleanup_production_enabled =
        result.runtime_indexed_cleanup_capability_state.any_production_enabled || promoted_member_cleanup_ready;
    auto const include_whole_element_cleanup_details =
        !promoted_member_cleanup_ready ||
        result.runtime_indexed_cleanup_capability_state.all_prerequisites_ready ||
        result.runtime_indexed_cleanup_capability_state.any_production_enabled;

    auto report = std::ostringstream {};
    report << "runtime-index cleanup constructor-move production-readiness "
           << "constructor-move " << (has_constructor_move_gate_diagnostic ? "blocked" : "enabled")
           << " partial-ownership " << (has_partial_ownership_diagnostic ? "required" : "accepted")
           << " cleanup-proof "
           << (cleanup_proof_ready ? "ready" : "blocked")
           << " cleanup-production "
           << (cleanup_production_enabled ? "enabled" : "disabled")
           << " capability-count " << result.runtime_indexed_cleanup_capability_state.capability_count
           << " ordinary-emit " << (result.has_errors() ? "rejected" : "accepted")
           << " member-cleanup-promotion " << member_cleanup_typed_promotion.state
           << " member-production-records " << member_cleanup_typed_promotion.production_readiness_count
           << " member-gate-records " << member_cleanup_typed_promotion.typed_gate_count
           << " member-mutation-records " << member_cleanup_typed_promotion.mutation_readiness_count
           << " member-rewrite-records " << member_cleanup_typed_promotion.rewrite_promotion_count
           << " diagnostic "
           << (has_constructor_move_gate_diagnostic ? "runtime-index constructor move gate disabled" : "none")
           << " member-module-ir-shape "
           << (member_cleanup_typed_promotion.module_ir_shape_ready ? "ready" : "blocked");
    if (!member_cleanup_typed_promotion.module_ir_shape_blocker_detail.empty()) {
        report << " member-module-ir-shape-detail "
               << member_cleanup_typed_promotion.module_ir_shape_blocker_detail;
    }
    if (include_whole_element_cleanup_details) {
        for (auto const& line : pipeline::runtime_indexed_constructor_move_plan_report_lines(
                 result.runtime_indexed_cleanup_emission_plan_state
             )) {
            report << '\n' << line;
        }
        for (auto const& line : pipeline::runtime_indexed_constructor_move_ir_shape_report_lines(
                 result.runtime_indexed_cleanup_emission_plan_state
             )) {
            report << '\n' << line;
        }
    }
    for (auto const& line : pipeline::runtime_indexed_member_cleanup_promotion_state_report_lines(result)) {
        report << '\n' << line;
    }
    for (auto const& line : pipeline::runtime_indexed_member_cleanup_readiness_report_lines(result)) {
        report << '\n' << line;
    }
    for (auto const& line : pipeline::runtime_indexed_member_cleanup_execution_summary_report_lines(result)) {
        report << '\n' << line;
    }
    return report.str();
}

auto runtime_indexed_cleanup_function_module_mutation_report(
    pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState const& state
) -> std::string {
    auto report = std::ostringstream {};
    report << "runtime-index cleanup function-module mutation "
           << "requested " << (state.mutation_requested ? "true" : "false")
           << " candidate-verified " << (state.candidate_verified ? "true" : "false")
           << " replacement-targets " << (state.replacement_targets_unique ? "unique" : "blocked")
           << " mutation-applied " << (state.mutation_applied ? "true" : "false")
           << " module-matches-candidate " << (state.module_matches_candidate ? "true" : "false")
           << " composition-failure "
           << pipeline::runtime_indexed_cleanup_ir_composition_failure_token(state.composition_failure);
    if (state.composition_failure_part_available) {
        report << " composition-part " << state.composition_failure_part_index
               << " splice-range " << state.composition_failure_splice_range.start_offset
               << ".." << state.composition_failure_splice_range.end_offset;
    }
    report << " apply-stages " << (state.rewrite_apply_stage_available ? "available" : "unavailable")
           << " branch-replacements " << (state.branch_replacements_applied ? "true" : "false")
           << " cleanup-cfg-appended " << (state.cleanup_cfg_appended ? "true" : "false")
           << " phi-retargeted " << (state.phi_predecessors_retargeted ? "true" : "false");
    report << " llvm-passed " << (state.llvm_verifier_passed ? "true" : "false")
           << " diagnostics " << state.llvm_verifier_diagnostic_count
           << " final-lines " << state.final_module_line_count;
    return report.str();
}

}  // namespace orison::driver
