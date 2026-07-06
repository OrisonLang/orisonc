#include "orison/lowering/statement_body_lowering.hpp"

#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/while_emitter.hpp"

#include <optional>
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

auto lower_common_nonvalue_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    BindingTypeInferer infer_binding_type,
    std::string_view unsupported_let_diagnostic,
    std::string_view unsupported_var_diagnostic
) -> std::optional<StatementFlow> {
    if (statement.kind == syntax::StatementKind::let_binding) {
        auto type = infer_binding_type(statement, context, session.state);
        if (!type.has_value()) {
            diagnostics.error(statement.line, std::string(unsupported_let_diagnostic));
            return StatementFlow::failed;
        }
        return lower_let_statement(
            statement,
            type->type,
            type->signedness,
            context,
            session,
            diagnostics,
            output
        ) ? StatementFlow::falls_through
          : StatementFlow::failed;
    }
    if (statement.kind == syntax::StatementKind::var_binding) {
        auto type = infer_binding_type(statement, context, session.state);
        if (!type.has_value()) {
            diagnostics.error(statement.line, std::string(unsupported_var_diagnostic));
            return StatementFlow::failed;
        }
        return lower_var_statement(
            statement,
            type->type,
            type->signedness,
            context,
            session,
            diagnostics,
            output
        ) ? StatementFlow::falls_through
          : StatementFlow::failed;
    }
    if (statement.kind == syntax::StatementKind::assignment_statement) {
        return lower_assignment_statement(statement, context, session, diagnostics, output)
            ? StatementFlow::falls_through
            : StatementFlow::failed;
    }
    if (statement.kind == syntax::StatementKind::expression_statement &&
        statement.expression.kind == syntax::ExpressionKind::call) {
        return lower_call_statement(statement, context, session, diagnostics, output)
            ? StatementFlow::falls_through
            : StatementFlow::failed;
    }
    if (statement.kind == syntax::StatementKind::defer_statement) {
        return record_deferred_cleanup(statement, session, diagnostics)
            ? StatementFlow::falls_through
            : StatementFlow::failed;
    }
    if (statement.kind == syntax::StatementKind::break_statement ||
        statement.kind == syntax::StatementKind::continue_statement) {
        return lower_loop_control_statement(statement, context, session, diagnostics, output)
            ? StatementFlow::terminated
            : StatementFlow::failed;
    }
    if (statement.kind == syntax::StatementKind::while_statement) {
        return lower_while_statement(statement, context, session, diagnostics, output)
            ? StatementFlow::falls_through
            : StatementFlow::failed;
    }
    return std::nullopt;
}

}  // namespace orison::lowering
