#pragma once

#include "orison/pipeline/runtime_indexed_cleanup_composition_failures.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_ranges.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct RuntimeIndexedCleanupFunctionIrRewriteCandidate {
    std::string function_symbol_name;
    std::string predecessor_block_name;
    std::string insertion_block_name;
    std::string continuation_block_name;
    std::string replaced_terminator_text;
    std::string inserted_branch_text;
    std::string original_function_ir_text;
    std::string candidate_function_ir_text;
    bool candidate_available = false;
    bool separate_from_module_ir = true;
    bool function_ir_changed = false;
    bool predecessor_terminator_replaced = false;
    bool splice_range_available = false;
    RuntimeIndexedCleanupIrCompositionFailure composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;
    std::size_t source_line = 0;
    std::size_t original_function_line_count = 0;
    std::size_t candidate_function_line_count = 0;
    std::size_t inserted_cfg_line_count = 0;
    RuntimeIndexedCleanupTextSpliceRange splice_range;
};

struct RuntimeIndexedCleanupFunctionIrRewriteCandidateState {
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate> candidates;
    bool metadata_available = false;
    bool any_candidate_available = false;
    bool all_candidates_separate_from_module_ir = true;
    bool all_splice_ranges_available = true;
    bool same_function_splice_ranges_ordered = true;
    bool same_function_splice_ranges_non_overlapping = true;
    bool any_function_ir_changed = false;
    RuntimeIndexedCleanupIrCompositionFailure first_composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;
    std::size_t candidate_count = 0;
    std::size_t available_candidate_count = 0;
    std::size_t composition_failure_count = 0;
    std::size_t original_function_line_count = 0;
    std::size_t candidate_function_line_count = 0;
    std::size_t inserted_cfg_line_count = 0;
};

struct RuntimeIndexedCleanupFunctionIrRewriteCandidateVerification {
    std::string function_symbol_name;
    std::string predecessor_block_name;
    std::string insertion_block_name;
    std::string continuation_block_name;
    bool verification_available = false;
    bool original_function_excludes_cleanup_cfg = false;
    bool candidate_contains_cleanup_cfg_once = false;
    bool candidate_contains_continuation_once = false;
    bool candidate_routes_predecessor_to_cleanup = false;
    bool original_predecessor_terminator_found = false;
    bool candidate_function_changed = false;
    bool predecessor_terminator_replaced = false;
    bool splice_range_available = false;
    bool separate_from_module_ir = false;
    bool verified = false;
    RuntimeIndexedCleanupIrCompositionFailure composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;
    std::size_t source_line = 0;
    std::size_t original_cleanup_block_count = 0;
    std::size_t candidate_cleanup_block_count = 0;
    std::size_t candidate_continuation_block_count = 0;
    std::size_t original_predecessor_terminator_count = 0;
    std::size_t candidate_predecessor_cleanup_branch_count = 0;
    RuntimeIndexedCleanupTextSpliceRange splice_range;
};

struct RuntimeIndexedCleanupFunctionIrRewriteCandidateVerificationState {
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidateVerification> verifications;
    bool verification_metadata_available = false;
    bool all_original_functions_exclude_cleanup_cfg = false;
    bool all_candidates_contain_cleanup_cfg_once = false;
    bool all_candidates_contain_continuation_once = false;
    bool all_candidates_route_predecessors_to_cleanup = false;
    bool all_original_predecessor_terminators_found = false;
    bool all_predecessor_terminators_replaced = false;
    bool all_candidate_functions_changed = false;
    bool all_candidates_separate_from_module_ir = false;
    bool all_splice_ranges_available = false;
    bool same_function_splice_ranges_ordered = false;
    bool same_function_splice_ranges_non_overlapping = false;
    bool all_verified = false;
    RuntimeIndexedCleanupIrCompositionFailure first_composition_failure =
        RuntimeIndexedCleanupIrCompositionFailure::none;
    std::size_t verification_count = 0;
    std::size_t verified_count = 0;
    std::size_t composition_failure_count = 0;
};

} // namespace orison::pipeline
