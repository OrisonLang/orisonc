#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <functional>
#include <span>
#include <sstream>
#include <string_view>

namespace orison::lowering {

using NonValueStatementLowerer = std::function<StatementFlow(
    syntax::StatementSyntax const& statement,
    bool is_last_statement
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

}  // namespace orison::lowering
