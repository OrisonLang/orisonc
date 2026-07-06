#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <functional>
#include <sstream>

namespace orison::lowering {

using ForLoopBodyLowerer = std::function<StatementFlow()>;

auto lower_fixed_array_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    ForLoopBodyLowerer lower_body
) -> StatementFlow;

}  // namespace orison::lowering
