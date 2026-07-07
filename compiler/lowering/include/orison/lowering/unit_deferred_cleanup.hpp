#pragma once

#include "orison/lowering/deferred_cleanup.hpp"

namespace orison::lowering {

auto lower_unit_deferred_cleanup_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

}  // namespace orison::lowering
