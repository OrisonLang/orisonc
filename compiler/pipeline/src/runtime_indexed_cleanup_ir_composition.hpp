#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct RuntimeIndexedCleanupFunctionIrCompositionPart {
    std::string predecessor_block_name;
    std::string continuation_block_name;
    std::string replacement_branch_text;
    std::string cleanup_cfg_tail;
    std::size_t splice_start_offset = 0;
    std::size_t splice_end_offset = 0;
};

auto build_runtime_indexed_cleanup_function_ir_composition_parts(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> const& candidates
) -> std::vector<RuntimeIndexedCleanupFunctionIrCompositionPart>;

auto compose_non_overlapping_function_ir_rewrite(
    std::string const& original_function_ir,
    std::vector<RuntimeIndexedCleanupFunctionIrRewriteCandidate const*> candidates
) -> std::string;

} // namespace orison::pipeline
