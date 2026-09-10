#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

namespace orison::pipeline {

void populate_semantic_owned_cleanup_reports(
    CompilePipelineResult& result,
    CompilePipelineOptions const& options
);

}  // namespace orison::pipeline
