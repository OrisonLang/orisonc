#include "orison/driver/runtime_indexed_cleanup_reports.hpp"

#include <cassert>
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
            .candidate_count = 3,
            .composition_failure_part_index = 2,
            .composition_failure_splice_start_offset = 144,
            .composition_failure_splice_end_offset = 188,
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
            .candidate_count = 1,
            .final_module_line_count = 42,
            .llvm_verifier_diagnostic_count = 0,
        }
    );

    assert(report.find("composition-failure none llvm-passed true diagnostics 0 final-lines 42") !=
        std::string::npos);
    assert(report.find("composition-part") == std::string::npos);
    assert(report.find("splice-range") == std::string::npos);
}

}  // namespace

auto main() -> int {
    assert_mutation_report_with_composition_detail();
    assert_mutation_report_without_composition_detail();
    return 0;
}
