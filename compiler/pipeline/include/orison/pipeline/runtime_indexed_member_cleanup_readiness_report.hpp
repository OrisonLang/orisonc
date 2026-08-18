#pragma once

#include "orison/pipeline/compile_pipeline_result.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct RuntimeIndexedMemberCleanupPromotionState {
    std::string state = "none";
    std::size_t production_readiness_count = 0;
    std::size_t typed_gate_count = 0;
    std::size_t mutation_readiness_count = 0;
    std::size_t rewrite_promotion_count = 0;
    bool module_ir_shape_ready = true;
};

auto runtime_indexed_member_cleanup_promotion_state(
    CompilePipelineResult const& result
) -> RuntimeIndexedMemberCleanupPromotionState;

auto runtime_indexed_member_cleanup_promotion_state_report_lines(
    CompilePipelineResult const& result
) -> std::vector<std::string>;

auto runtime_indexed_member_cleanup_readiness_report_lines(
    CompilePipelineResult const& result
) -> std::vector<std::string>;

}  // namespace orison::pipeline
