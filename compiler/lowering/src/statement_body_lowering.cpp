#include "orison/lowering/statement_body_lowering.hpp"

#include "orison/lowering/statement_emitter.hpp"

#include <string>

namespace orison::lowering {

auto lower_nonvalue_statement_block(
    std::span<syntax::StatementSyntax const* const> statements,
    std::string_view terminated_statement_diagnostic,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    NonValueStatementLowerer lower_statement
) -> StatementFlow {
    auto flow = StatementFlow::falls_through;
    DeferredCleanupScope defer_scope(session.state);
    for (auto index = std::size_t {0}; index < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr) {
            return StatementFlow::failed;
        }
        if (flow == StatementFlow::terminated) {
            diagnostics.error(statement->line, std::string(terminated_statement_diagnostic));
            return StatementFlow::failed;
        }

        auto const is_last_statement = index + 1 == statements.size();
        flow = lower_statement(*statement, is_last_statement);
        if (flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
    }
    if (flow == StatementFlow::falls_through &&
        !emit_deferred_cleanup_to_depth(
            defer_scope.cleanup_depth(),
            context,
            session,
            diagnostics,
            output
        )) {
        return StatementFlow::failed;
    }
    return flow;
}

}  // namespace orison::lowering
