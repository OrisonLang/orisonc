#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct RuntimeIndexedCleanupModuleIrArtifactState {
    std::vector<std::string> rendered_ir_lines;
    bool artifact_available = false;
    bool separate_from_module_ir = true;
    std::size_t rendered_ir_line_count = 0;
};

struct RuntimeIndexedCleanupModuleIrInsertionGateState {
    bool insertion_requested = false;
    bool artifact_available = false;
    bool render_parity_verified = false;
    bool insertion_enabled = false;
    bool remains_separate_from_module_ir = true;
};

struct RuntimeIndexedCleanupModuleIrInsertionPreviewState {
    bool preview_available = false;
    bool insertion_point_found = false;
    bool would_modify_module_ir = false;
    std::size_t insertion_line_index = 0;
    std::size_t original_module_line_count = 0;
    std::size_t inserted_ir_line_count = 0;
    std::size_t projected_module_line_count = 0;
};

struct RuntimeIndexedCleanupModuleIrCandidateState {
    std::string candidate_ir_text;
    bool candidate_available = false;
    bool separate_from_module_ir = true;
    std::size_t original_module_line_count = 0;
    std::size_t candidate_module_line_count = 0;
    std::size_t inserted_ir_line_count = 0;
};

struct RuntimeIndexedCleanupModuleIrCandidateVerificationState {
    bool verification_available = false;
    bool candidate_contains_cleanup_block_once = false;
    bool emitted_module_excludes_cleanup_block = false;
    bool verified = false;
    std::size_t candidate_cleanup_block_count = 0;
    std::size_t emitted_module_cleanup_block_count = 0;
};

struct RuntimeIndexedCleanupModuleIrMutationState {
    bool mutation_requested = false;
    bool candidate_verified = false;
    bool mutation_applied = false;
    bool module_matches_candidate = false;
    std::size_t final_module_cleanup_block_count = 0;
    std::size_t final_module_line_count = 0;
};

} // namespace orison::pipeline
