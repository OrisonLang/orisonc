#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct RuntimeIndexedCleanupFunctionCfgRewritePlan {
    std::string function_symbol_name;
    std::string owner_name;
    std::string predecessor_block_name;
    std::string insertion_block_name;
    std::string continuation_block_name;
    std::string replaced_terminator_text;
    std::string inserted_branch_text;
    std::string continuation_block_text;
    std::vector<std::string> candidate_cfg_lines;
    bool target_known = false;
    bool rewrite_candidate_available = false;
    bool continuation_block_generated = false;
    bool function_ir_unchanged = true;
    std::size_t source_line = 0;
    std::size_t cleanup_slice_line_count = 0;
    std::size_t candidate_cfg_line_count = 0;
};

struct RuntimeIndexedCleanupFunctionCfgRewritePlanState {
    std::vector<RuntimeIndexedCleanupFunctionCfgRewritePlan> plans;
    bool metadata_available = false;
    bool all_targets_known = false;
    bool any_rewrite_candidate_available = false;
    bool any_continuation_block_generated = false;
    bool function_ir_unchanged = true;
    std::size_t plan_count = 0;
    std::size_t rewrite_candidate_count = 0;
    std::size_t cleanup_slice_line_count = 0;
    std::size_t candidate_cfg_line_count = 0;
};

struct RuntimeIndexedCleanupFunctionCfgRewriteVerification {
    std::string function_symbol_name;
    std::string predecessor_block_name;
    std::string insertion_block_name;
    std::string continuation_block_name;
    bool verification_available = false;
    bool function_found = false;
    bool predecessor_block_found = false;
    bool insertion_block_absent = false;
    bool continuation_block_found = false;
    bool candidate_insertion_block_found = false;
    bool candidate_continuation_block_found = false;
    bool candidate_verified = false;
    bool verified = false;
};

struct RuntimeIndexedCleanupFunctionCfgRewriteVerificationState {
    std::vector<RuntimeIndexedCleanupFunctionCfgRewriteVerification> verifications;
    bool verification_metadata_available = false;
    bool all_functions_found = false;
    bool all_predecessor_blocks_found = false;
    bool all_insertion_blocks_absent = false;
    bool all_continuation_blocks_found = false;
    bool all_candidate_insertion_blocks_found = false;
    bool all_candidate_continuation_blocks_found = false;
    bool all_candidates_verified = false;
    bool all_verified = false;
    std::size_t verification_count = 0;
    std::size_t candidate_verified_count = 0;
    std::size_t verified_count = 0;
};

} // namespace orison::pipeline
