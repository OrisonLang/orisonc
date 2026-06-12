#pragma once

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowering_failures.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace orison::lowering {

struct LoweredType {
    std::string type;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

struct ExpressionEmissionContext {
    LoweringContext const& lowering;
    StringConstantTable const& string_constants;
};

auto lower_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    ExpressionEmissionContext const& context,
    FunctionLoweringState& state,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto infer_expression_type(
    syntax::ExpressionSyntax const& expression,
    ExpressionEmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType>;

auto lower_integer_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness
) -> std::optional<LoweredExpression>;

auto lower_boolean_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression>;

auto is_integer_llvm_type(std::string_view type) -> bool;

}  // namespace orison::lowering
