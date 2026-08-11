#pragma once

#include "orison/pipeline/runtime_indexed_cleanup_composition_failures.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_ranges.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct RuntimeIndexedCleanupFunctionIrModuleRewriteCandidate {
    std::string function_symbol_name;
    std::string candidate_module_ir_text;
    bool rewrite_requested = false;
    bool function_candidate_verified = false;
    bool candidate_available = false;
    bool separate_from_module_ir = true;
    bool module_ir_changed = false;
    std::size_t source_line = 0;
    std::size_t original_module_line_count = 0;
    std::size_t candidate_module_line_count = 0;
    std::size_t function_replacement_count = 0;
};

struct RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateState {
    std::vector<RuntimeIndexedCleanupFunctionIrModuleRewriteCandidate> candidates;
    bool rewrite_requested = false;
    bool metadata_available = false;
    bool any_candidate_available = false;
    bool all_candidates_separate_from_module_ir = true;
    bool any_module_ir_changed = false;
    RuntimeIndexedCleanupIrCompositionFailure first_composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;
    std::size_t candidate_count = 0;
    std::size_t available_candidate_count = 0;
    std::size_t composition_failure_count = 0;
    std::size_t original_module_line_count = 0;
    std::size_t candidate_module_line_count = 0;
};

struct RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerification {
    std::string function_symbol_name;
    std::string llvm_verifier_diagnostic_text;
    bool verification_available = false;
    bool candidate_function_found = false;
    bool candidate_function_matches_verified_candidate = false;
    bool replacement_target_unique = false;
    bool module_ir_changed = false;
    bool separate_from_module_ir = false;
    bool llvm_verifier_ran = false;
    bool llvm_verifier_passed = false;
    bool verified = false;
    std::size_t source_line = 0;
    std::size_t function_replacement_count = 0;
    std::size_t llvm_verifier_diagnostic_count = 0;
};

struct RuntimeIndexedCleanupFunctionIrModuleRewriteSpliceConflict {
    std::string function_symbol_name;
    std::size_t left_candidate_index = 0;
    std::size_t right_candidate_index = 0;
    std::size_t left_source_line = 0;
    std::size_t right_source_line = 0;
    RuntimeIndexedCleanupTextSpliceRange left_splice_range;
    RuntimeIndexedCleanupTextSpliceRange right_splice_range;
};

struct RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerificationState {
    std::vector<RuntimeIndexedCleanupFunctionIrModuleRewriteCandidateVerification> verifications;
    std::vector<RuntimeIndexedCleanupFunctionIrModuleRewriteSpliceConflict> splice_conflicts;
    bool verification_metadata_available = false;
    bool all_candidate_functions_found = false;
    bool all_candidate_functions_match_verified_candidates = false;
    bool all_replacement_targets_unique = false;
    bool all_module_ir_changed = false;
    bool all_candidates_separate_from_module_ir = false;
    bool same_function_splice_ranges_non_overlapping = false;
    bool any_llvm_verifier_ran = false;
    bool all_llvm_verifier_passed = false;
    bool all_verified = false;
    RuntimeIndexedCleanupIrCompositionFailure first_composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;
    std::size_t verification_count = 0;
    std::size_t verified_count = 0;
    std::size_t llvm_verified_count = 0;
    std::size_t splice_conflict_count = 0;
    std::size_t composition_failure_count = 0;
    std::size_t llvm_verifier_diagnostic_count = 0;
};

struct RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState {
    bool mutation_requested = false;
    bool candidate_verified = false;
    bool replacement_targets_unique = false;
    bool mutation_applied = false;
    bool module_matches_candidate = false;
    bool llvm_verifier_passed = false;
    RuntimeIndexedCleanupIrCompositionFailure composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;
    bool composition_failure_part_available = false;
    bool rewrite_apply_stage_available = false;
    bool branch_replacements_applied = false;
    bool cleanup_cfg_appended = false;
    bool phi_predecessors_retargeted = false;
    std::size_t candidate_count = 0;
    std::size_t composition_failure_part_index = 0;
    RuntimeIndexedCleanupTextSpliceRange composition_failure_splice_range;
    std::size_t final_module_line_count = 0;
    std::size_t llvm_verifier_diagnostic_count = 0;
};

} // namespace orison::pipeline
