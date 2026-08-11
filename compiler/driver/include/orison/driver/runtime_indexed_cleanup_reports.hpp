#pragma once

#include "orison/pipeline/runtime_indexed_cleanup_module_ir_rewrite_candidates.hpp"

#include <string>

namespace orison::driver {

auto runtime_indexed_cleanup_function_module_mutation_report(
    pipeline::RuntimeIndexedCleanupFunctionIrModuleRewriteMutationState const& state
) -> std::string;

}  // namespace orison::driver
