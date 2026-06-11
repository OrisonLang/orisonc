#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/syntax/module_parser.hpp"

#include <string>

namespace orison::lowering {

auto emit_function(
    syntax::FunctionSyntax const& function,
    LoweringContext const& lowering_context,
    StringConstantTable const& string_constants,
    diagnostics::DiagnosticBag& diagnostics
) -> std::string;

}  // namespace orison::lowering
