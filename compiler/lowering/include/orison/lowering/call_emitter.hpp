#pragma once

#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace orison::lowering {

auto lower_call_arguments(
    syntax::ExpressionSyntax const& expression,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<std::vector<LoweredExpression>>;

auto lower_member_call_arguments(
    syntax::ExpressionSyntax const& receiver_expression,
    std::span<syntax::ExpressionSyntax const> arguments,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<std::vector<LoweredExpression>>;

auto lower_member_call_arguments(
    LoweredExpression receiver,
    std::span<syntax::ExpressionSyntax const> arguments,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<std::vector<LoweredExpression>>;

auto emit_value_call(
    std::string temporary_name,
    LoweredFunctionSignature const& function,
    std::vector<LoweredExpression> const& arguments,
    std::ostringstream& output
) -> LoweredExpression;

auto emit_void_call(
    LoweredFunctionSignature const& function,
    std::vector<LoweredExpression> const& arguments,
    std::ostringstream& output
) -> void;

}  // namespace orison::lowering
