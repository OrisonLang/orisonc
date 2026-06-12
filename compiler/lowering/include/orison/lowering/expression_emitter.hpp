#pragma once

#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
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

auto lower_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto infer_expression_type(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
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
