#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

namespace orison::pipeline {

auto run_dynamic_array_cleanup_metadata_stage(
    CompilePipeline const& pipeline,
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) -> CompilePipelineResult;

}  // namespace orison::pipeline
