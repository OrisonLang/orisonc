#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

namespace orison::pipeline {

auto plan_dynamic_array_cleanup_production_readiness(
    CompilePipelineResult const& result,
    CompilePipelineOptions const& options
) -> DynamicArrayCleanupProductionReadiness;

}  // namespace orison::pipeline
