#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <functional>
#include <optional>
#include <sstream>

namespace orison::lowering {

using ForLoopBodyLowerer = std::function<StatementFlow()>;
using ForLoopElementTypeInferer = std::function<std::optional<LoweredType>(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
)>;

auto lower_fixed_array_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    ForLoopBodyLowerer lower_body
) -> StatementFlow;

auto lower_array_literal_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    ForLoopElementTypeInferer infer_element_type,
    ForLoopBodyLowerer lower_body
) -> StatementFlow;

}  // namespace orison::lowering
