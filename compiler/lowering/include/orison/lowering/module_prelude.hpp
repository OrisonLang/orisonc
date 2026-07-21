#pragma once

#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/drop_metadata.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/string_constants.hpp"

#include <string>
#include <vector>

namespace orison::lowering {

auto emit_module_prelude(
    StringConstantTable const& string_constants,
    std::vector<LoweredFunctionSignature> const& foreign_declarations,
    std::vector<ConcurrencyRuntimeOperation> const& concurrency_runtime_operations = {},
    std::vector<PlannedDropDeclaration> const& planned_drop_declarations = {},
    std::vector<DynamicArrayRuntimeOperation> const& dynamic_array_runtime_operations = {},
    std::vector<std::string> const& source_defined_drop_symbols = {}
) -> std::string;

}  // namespace orison::lowering
