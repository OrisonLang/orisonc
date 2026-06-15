#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/syntax/module_parser.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace orison::lowering {

struct LoweredMethodSignature {
    std::string receiver_type_name;
    std::string method_name;
    LoweredFunctionSignature signature;
};

struct LoweringContext {
    std::unordered_map<std::string, LoweredFunctionSignature> functions;
    std::vector<LoweredMethodSignature> methods;
    std::vector<LoweredFunctionSignature> foreign_declarations;
};

auto build_lowering_context(
    syntax::ModuleSyntax const& module,
    diagnostics::DiagnosticBag& diagnostics
) -> LoweringContext;

}  // namespace orison::lowering
