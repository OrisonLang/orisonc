#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

namespace orison::pipeline {

auto run_llvm_emission_stage(
    CompilePipeline const& pipeline,
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) -> CompilePipelineResult;

auto run_object_emission_stage(
    CompilePipeline const& pipeline,
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) -> CompilePipelineResult;

}  // namespace orison::pipeline
