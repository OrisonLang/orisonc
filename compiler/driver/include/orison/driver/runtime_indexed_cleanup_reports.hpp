#pragma once

#include "orison/pipeline/compile_pipeline_result.hpp"
#include "orison/pipeline/runtime_indexed_cleanup_module_ir_rewrite_candidates.hpp"

#include <string>

namespace orison::driver {

auto runtime_indexed_constructor_move_production_readiness_report(
    pipeline::CompilePipelineResult const& result
) -> std::string;

auto runtime_indexed_cleanup_function_module_mutation_report(
    pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState const& state
) -> std::string;

}  // namespace orison::driver
