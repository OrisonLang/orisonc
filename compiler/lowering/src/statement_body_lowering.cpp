#include "orison/lowering/statement_body_lowering.hpp"

#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/while_emitter.hpp"

namespace orison::lowering {

auto lower_common_builtin_nonvalue_statement(
    syntax::StatementSyntax const& statement,
    std::optional<LoweredType> binding_type,
    std::string_view unsupported_let_diagnostic,
    std::string_view unsupported_var_diagnostic,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> std::optional<StatementFlow> {
    if (statement.kind == syntax::StatementKind::let_binding) {
        if (!binding_type.has_value()) {
            diagnostics.error(statement.line, std::string(unsupported_let_diagnostic));
            return StatementFlow::failed;
        }
        return lower_let_statement(
            statement,
            binding_type->type,
            binding_type->signedness,
            context,
            session,
            diagnostics,
            output
        ) ? StatementFlow::falls_through
          : StatementFlow::failed;
    }
    if (statement.kind == syntax::StatementKind::var_binding) {
        if (!binding_type.has_value()) {
            diagnostics.error(statement.line, std::string(unsupported_var_diagnostic));
            return StatementFlow::failed;
        }
        return lower_var_statement(
            statement,
            binding_type->type,
            binding_type->signedness,
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
        return lower_loop_control_statement(
            statement,
            context,
            session,
            diagnostics,
            output,
            lower_cleanup_block
        )
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
