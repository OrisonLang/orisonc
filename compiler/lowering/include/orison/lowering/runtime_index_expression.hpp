#pragma once

#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace orison::lowering {

auto runtime_index_expression_key(syntax::ExpressionSyntax const& expression) -> std::string;

auto decimal_integer_literal_text(
    syntax::ExpressionSyntax const& expression
) -> std::optional<std::string_view>;

auto is_runtime_index_expression(syntax::ExpressionSyntax const& expression) -> bool;

auto contains_runtime_indexed_projection(syntax::ExpressionSyntax const& expression) -> bool;

}  // namespace orison::lowering
