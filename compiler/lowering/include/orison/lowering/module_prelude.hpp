#pragma once

#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/string_constants.hpp"

#include <string>
#include <vector>

namespace orison::lowering {

auto emit_module_prelude(
    StringConstantTable const& string_constants,
    std::vector<LoweredFunctionSignature> const& foreign_declarations
) -> std::string;

}  // namespace orison::lowering
