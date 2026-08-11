#include "orison/driver/runtime_indexed_cleanup_reports.hpp"

#include <sstream>

namespace orison::driver {

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
               << " splice-range " << state.composition_failure_splice_start_offset
               << ".." << state.composition_failure_splice_end_offset;
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
