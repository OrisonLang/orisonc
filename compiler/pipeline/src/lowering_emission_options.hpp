#pragma once

#include "orison/lowering/lowering_options.hpp"
#include "orison/pipeline/compile_pipeline.hpp"

namespace orison::pipeline {

enum class LoweringEmissionMode {
    full_ir,
    dynamic_array_cleanup_metadata,
};

auto dynamic_array_construction_lowering_enabled(CompilePipelineOptions const& options) -> bool;
auto dynamic_array_parameter_lowering_enabled(CompilePipelineOptions const& options) -> bool;
auto dynamic_array_index_lowering_enabled(CompilePipelineOptions const& options) -> bool;
auto dynamic_array_length_lowering_enabled(CompilePipelineOptions const& options) -> bool;
auto dynamic_array_for_lowering_enabled(CompilePipelineOptions const& options) -> bool;
auto dynamic_array_append_lowering_enabled(CompilePipelineOptions const& options) -> bool;
auto dynamic_array_cleanup_emission_enabled(CompilePipelineOptions const& options) -> bool;
auto source_drop_lowering_enabled(CompilePipelineOptions const& options) -> bool;
auto dynamic_array_descriptor_cleanup_planning_enabled(CompilePipelineOptions const& options) -> bool;

auto build_lowering_emission_options(
    CompilePipelineResult const& result,
    CompilePipelineOptions const& options,
    LoweringEmissionMode mode
) -> lowering::LlvmIrEmissionOptions;

}  // namespace orison::pipeline
