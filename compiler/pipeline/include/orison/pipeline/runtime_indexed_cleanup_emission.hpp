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

struct RuntimeIndexedCleanupPlanParitySummary {
    std::string left_owner_name;
    std::string right_owner_name;
    bool index_expression_matches = false;
    bool element_source_type_matches = false;
    bool element_llvm_type_matches = false;
    bool element_size_matches = false;
    bool drop_callee_matches = false;
    bool operation_sequence_matches = false;
    bool owner_llvm_type_differs = false;
    bool static_length_differs = false;
    bool descriptor_owner_readiness_differs = false;
    bool shared_metadata_matches = false;
    bool storage_metadata_differs = false;

    auto operator==(RuntimeIndexedCleanupPlanParitySummary const&) const -> bool = default;
};

struct RuntimeIndexedCleanupIrShapeSummary {
    std::string owner_name;
    std::size_t gated_ir_slice_line_count = 0;
    std::size_t condition_block_count = 0;
    std::size_t live_check_block_count = 0;
    std::size_t skip_block_count = 0;
    std::size_t drop_block_count = 0;
    std::size_t continue_block_count = 0;
    std::size_t exit_block_count = 0;
    bool branch_to_condition_found = false;
    bool bounds_check_found = false;
    bool skip_check_found = false;
    bool drop_call_found = false;
    bool next_index_found = false;
    bool descriptor_load_found = false;
    bool descriptor_element_gep_found = false;
    bool inline_element_gep_found = false;
    bool zero_store_found = false;
    bool deallocate_call_found = false;
    bool common_loop_shape_ready = false;
    bool descriptor_storage_shape_ready = false;
    bool inline_storage_shape_ready = false;

    auto operator==(RuntimeIndexedCleanupIrShapeSummary const&) const -> bool = default;
};

struct RuntimeIndexedCleanupIrShapeParitySummary {
    std::string left_owner_name;
    std::string right_owner_name;
    bool common_loop_shape_matches = false;
    bool drop_call_shape_matches = false;
    bool storage_ir_shape_differs = false;
    bool cleanup_tail_differs = false;
    bool line_count_differs = false;

    auto operator==(RuntimeIndexedCleanupIrShapeParitySummary const&) const -> bool = default;
};

auto runtime_indexed_constructor_move_plan_report(
    lowering::RuntimeIndexedCleanupEmissionPlan const& plan
) -> std::string;

auto runtime_indexed_constructor_move_plan_report_lines(
    RuntimeIndexedCleanupEmissionPlanState const& state
) -> std::vector<std::string>;

auto runtime_indexed_cleanup_plan_parity_summary(
    lowering::RuntimeIndexedCleanupEmissionPlan const& left,
    lowering::RuntimeIndexedCleanupEmissionPlan const& right
) -> RuntimeIndexedCleanupPlanParitySummary;

auto runtime_indexed_cleanup_plan_parity_summary_report(
    RuntimeIndexedCleanupPlanParitySummary const& summary
) -> std::string;

auto runtime_indexed_cleanup_ir_shape_summary(
    lowering::RuntimeIndexedCleanupEmissionPlan const& plan
) -> RuntimeIndexedCleanupIrShapeSummary;

auto runtime_indexed_cleanup_ir_shape_parity_summary(
    lowering::RuntimeIndexedCleanupEmissionPlan const& left,
    lowering::RuntimeIndexedCleanupEmissionPlan const& right
) -> RuntimeIndexedCleanupIrShapeParitySummary;

auto runtime_indexed_cleanup_ir_shape_parity_summary_report(
    RuntimeIndexedCleanupIrShapeParitySummary const& summary
) -> std::string;

auto runtime_indexed_constructor_move_ir_shape_report(
    lowering::RuntimeIndexedCleanupEmissionPlan const& plan
) -> std::string;

auto runtime_indexed_constructor_move_ir_shape_report_lines(
    RuntimeIndexedCleanupEmissionPlanState const& state
) -> std::vector<std::string>;

} // namespace orison::pipeline
