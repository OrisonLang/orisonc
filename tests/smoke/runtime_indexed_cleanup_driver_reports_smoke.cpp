#include "orison/driver/runtime_indexed_cleanup_reports.hpp"
#include "orison/pipeline/compile_pipeline.hpp"

#include <cassert>
#include <filesystem>
#include <string>

namespace {

namespace driver = orison::driver;
namespace pipeline = orison::pipeline;

void assert_mutation_report_with_composition_detail() {
    auto const report = driver::runtime_indexed_cleanup_function_module_mutation_report(
        pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
            .mutation_requested = true,
            .candidate_verified = true,
            .replacement_targets_unique = true,
            .mutation_applied = false,
            .module_matches_candidate = false,
            .llvm_verifier_passed = false,
            .composition_failure = pipeline::RuntimeIndexedCleanupIrCompositionFailure::invalid_candidate,
            .composition_failure_part_available = true,
            .rewrite_apply_stage_available = true,
            .branch_replacements_applied = true,
            .cleanup_cfg_appended = false,
            .phi_predecessors_retargeted = false,
            .candidate_count = 3,
            .composition_failure_part_index = 2,
            .composition_failure_splice_range = pipeline::RuntimeIndexedCleanupTextSpliceRange {
                .start_offset = 144,
                .end_offset = 188,
            },
            .final_module_line_count = 91,
            .llvm_verifier_diagnostic_count = 0,
        }
    );

    assert(report.find("runtime-index cleanup function-module mutation requested true") != std::string::npos);
    assert(report.find("candidate-verified true replacement-targets unique") != std::string::npos);
    assert(report.find("mutation-applied false module-matches-candidate false") != std::string::npos);
    assert(
        report.find(
            "composition-failure invalid-candidate composition-part 2 splice-range 144..188 "
            "apply-stages available branch-replacements true cleanup-cfg-appended false phi-retargeted false "
            "llvm-passed false diagnostics 0 final-lines 91"
        ) != std::string::npos
    );
}

void assert_mutation_report_without_composition_detail() {
    auto const report = driver::runtime_indexed_cleanup_function_module_mutation_report(
        pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
            .mutation_requested = true,
            .candidate_verified = true,
            .replacement_targets_unique = true,
            .mutation_applied = true,
            .module_matches_candidate = true,
            .llvm_verifier_passed = true,
            .composition_failure = pipeline::RuntimeIndexedCleanupIrCompositionFailure::none,
            .composition_failure_part_available = false,
            .rewrite_apply_stage_available = false,
            .branch_replacements_applied = false,
            .cleanup_cfg_appended = false,
            .phi_predecessors_retargeted = false,
            .candidate_count = 1,
            .final_module_line_count = 42,
            .llvm_verifier_diagnostic_count = 0,
        }
    );

    assert(
        report.find(
            "composition-failure none apply-stages unavailable branch-replacements false "
            "cleanup-cfg-appended false phi-retargeted false llvm-passed true diagnostics 0 final-lines 42"
        ) != std::string::npos
    );
    assert(report.find("composition-part") == std::string::npos);
    assert(report.find("splice-range") == std::string::npos);
}

void assert_mutation_report_for_scalar_same_function_success() {
    auto const report = driver::runtime_indexed_cleanup_function_module_mutation_report(
        pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
            .mutation_requested = true,
            .candidate_verified = true,
            .replacement_targets_unique = true,
            .mutation_applied = true,
            .module_matches_candidate = true,
            .llvm_verifier_passed = true,
            .composition_failure = pipeline::RuntimeIndexedCleanupIrCompositionFailure::none,
            .composition_failure_part_available = false,
            .rewrite_apply_stage_available = true,
            .branch_replacements_applied = true,
            .cleanup_cfg_appended = true,
            .phi_predecessors_retargeted = true,
            .candidate_count = 2,
            .final_module_line_count = 176,
            .llvm_verifier_diagnostic_count = 0,
        }
    );

    assert(
        report ==
        "runtime-index cleanup function-module mutation requested true candidate-verified true "
        "replacement-targets unique mutation-applied true module-matches-candidate true "
        "composition-failure none apply-stages available branch-replacements true "
        "cleanup-cfg-appended true phi-retargeted true llvm-passed true diagnostics 0 final-lines 176"
    );
    assert(report.find("composition-part") == std::string::npos);
    assert(report.find("splice-range") == std::string::npos);
}

void assert_constructor_move_report_for_two_member_cleanup_ready() {
    auto options = pipeline::production_compile_pipeline_options();
    options.collect_runtime_indexed_cleanup_audit = true;
    auto const result = pipeline::CompilePipeline {}.emit_llvm(
        std::filesystem::path(ORISON_SOURCE_DIR) / "tests" / "fixtures" /
            "runtime_indexed_dynamic_array_constructor_two_computed_member_transfers.or",
        options
    );
    auto const report = driver::runtime_indexed_constructor_move_production_readiness_report(result);

    assert(
        report.find(
            "runtime-index cleanup constructor-move production-readiness "
            "constructor-move enabled partial-ownership accepted cleanup-proof ready cleanup-production enabled "
            "capability-count 2 ordinary-emit accepted member-cleanup-promotion ready "
            "member-production-records 2 member-gate-records 2 member-mutation-records 2 "
            "member-rewrite-records 2 diagnostic none member-module-ir-shape ready"
        ) != std::string::npos
    );
    assert(report.find("diagnostic none member-module-ir-shape ready") != std::string::npos);
    assert(report.find("member-module-ir-shape-detail") == std::string::npos);
}

}  // namespace

auto main() -> int {
    assert_mutation_report_with_composition_detail();
    assert_mutation_report_without_composition_detail();
    assert_mutation_report_for_scalar_same_function_success();
    assert_constructor_move_report_for_two_member_cleanup_ready();
    return 0;
}
