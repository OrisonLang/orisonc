#pragma once

#include "orison/pipeline/compile_pipeline.hpp"

#include <string>

namespace orison::driver {

auto runtime_indexed_cleanup_function_module_mutation_report(
    pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState const& state
) -> std::string;

}  // namespace orison::driver
