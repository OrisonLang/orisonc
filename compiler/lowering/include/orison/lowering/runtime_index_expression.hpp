#pragma once

#include "orison/syntax/module_parser.hpp"

#include <string>

namespace orison::lowering {

auto runtime_index_expression_key(syntax::ExpressionSyntax const& expression) -> std::string;

}  // namespace orison::lowering
