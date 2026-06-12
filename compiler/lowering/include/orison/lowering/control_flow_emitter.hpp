#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_lowering_state.hpp"
#include "orison/syntax/module_parser.hpp"

#include <optional>
#include <sstream>
#include <string_view>

namespace orison::lowering {

auto value_expression_for(
    syntax::StatementSyntax const& statement
) -> syntax::ExpressionSyntax const*;

auto lower_let_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    ExpressionEmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool;

auto lower_final_control_flow_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    ExpressionEmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto render_control_flow_lowering_failure(
    ControlFlowLoweringFailure const& failure
) -> std::string;

}  // namespace orison::lowering
