#pragma once

#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/string_constants.hpp"

#include <vector>

namespace orison::lowering {

auto emit_module_prelude(
    StringConstantTable const& string_constants,
    std::vector<LoweredFunctionSignature> const& foreign_declarations,
    std::vector<ConcurrencyRuntimeOperation> const& concurrency_runtime_operations = {},
    std::vector<PlannedDropDeclaration> const& drop_declarations = {}
) -> std::string;

}  // namespace orison::lowering
