#pragma once

#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/string_constants.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace orison::lowering {

struct DropPreludeDeclaration {
    std::string symbol_name;
    std::string source_type_name;
    std::size_t discovery_line = 0;
    bool emit_declaration = false;
};

auto emit_module_prelude(
    StringConstantTable const& string_constants,
    std::vector<LoweredFunctionSignature> const& foreign_declarations,
    std::vector<ConcurrencyRuntimeOperation> const& concurrency_runtime_operations = {},
    std::vector<DropPreludeDeclaration> const& drop_declarations = {}
) -> std::string;

}  // namespace orison::lowering
