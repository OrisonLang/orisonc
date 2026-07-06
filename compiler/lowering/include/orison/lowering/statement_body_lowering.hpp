#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <functional>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>

namespace orison::lowering {

using NonValueStatementLowerer = std::function<StatementFlow(
    syntax::StatementSyntax const& statement,
    bool is_last_statement
)>;
using BindingTypeInferer = std::function<std::optional<LoweredType>(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
)>;

auto lower_nonvalue_statement_block(
    std::span<syntax::StatementSyntax const* const> statements,
    std::string_view terminated_statement_diagnostic,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    NonValueStatementLowerer lower_statement
) -> StatementFlow;

auto lower_common_nonvalue_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    BindingTypeInferer infer_binding_type,
    std::string_view unsupported_let_diagnostic,
    std::string_view unsupported_var_diagnostic
) -> std::optional<StatementFlow>;

}  // namespace orison::lowering
