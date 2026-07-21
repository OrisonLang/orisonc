#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

namespace orison::pipeline {

auto run_semantic_analysis_stage(
    std::filesystem::path const& source_path,
    CompilePipelineOptions const& options
) -> CompilePipelineResult;

}  // namespace orison::pipeline
