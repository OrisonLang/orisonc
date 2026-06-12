#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/syntax/module_parser.hpp"

#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace orison::lowering {

using FinalControlFlowLowerer = std::optional<LoweredExpression> (*)(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
);

auto value_expression_for(
    syntax::StatementSyntax const& statement
) -> syntax::ExpressionSyntax const*;

auto lower_let_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool;

auto lower_var_statement(
    syntax::StatementSyntax const& statement,
    std::string_view fallback_llvm_type,
    IntegerSignedness fallback_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool;

auto lower_assignment_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool;

auto lower_call_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool;

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow
) -> std::optional<LoweredExpression>;

auto lower_value_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow
) -> std::optional<LoweredExpression>;

}  // namespace orison::lowering
