#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

namespace orison::pipeline {

void populate_semantic_drop_reports(
    CompilePipelineResult& result,
    CompilePipelineOptions const& options
);

}  // namespace orison::pipeline
