#pragma once

#include "orison/lowering/ownership_transfer.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::pipeline {

struct RuntimeIndexedCleanupCapabilityState {
    std::vector<lowering::RuntimeIndexedCleanupCapability> capabilities;
    bool capability_metadata_available = false;
    bool all_prerequisites_ready = false;
    bool any_production_enabled = false;
    std::size_t capability_count = 0;
};

struct RuntimeIndexedCleanupEmissionPlanState {
    std::vector<lowering::RuntimeIndexedCleanupEmissionPlan> plans;
    bool plan_metadata_available = false;
    bool all_prerequisites_ready = false;
    bool any_production_gate_requested = false;
    bool any_production_enabled = false;
    bool any_length_load_slice_lowerable = false;
    bool any_loop_block_slice_lowerable = false;
    bool any_skip_branch_slice_lowerable = false;
    bool any_live_element_drop_slice_lowerable = false;
    bool any_cleanup_tail_slice_lowerable = false;
    bool any_structured_ir_plan_complete = false;
    bool all_function_insertion_targets_known = false;
    bool any_function_insertion_planned = false;
    std::size_t plan_count = 0;
    std::size_t operation_count = 0;
    std::size_t comment_ir_preview_line_count = 0;
    std::size_t gated_ir_slice_line_count = 0;
    std::size_t structured_ir_plan_count = 0;
    std::size_t function_insertion_plan_count = 0;
};

struct RuntimeIndexedCleanupIrRenderState {
    std::vector<std::string> rendered_ir_lines;
    bool render_metadata_available = false;
    bool all_structured_plans_complete = false;
    bool all_rendered_lines_match_artifact = false;
    std::size_t plan_count = 0;
    std::size_t rendered_plan_count = 0;
    std::size_t rendered_ir_line_count = 0;
};

auto runtime_indexed_constructor_move_plan_report(
    lowering::RuntimeIndexedCleanupEmissionPlan const& plan
) -> std::string;

auto runtime_indexed_constructor_move_plan_report_lines(
    RuntimeIndexedCleanupEmissionPlanState const& state
) -> std::vector<std::string>;

} // namespace orison::pipeline
