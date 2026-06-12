#include "orison/lowering/while_emitter.hpp"

#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/statement_emitter.hpp"

#include <string>

namespace orison::lowering {

auto lower_while_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (statement.kind != syntax::StatementKind::while_statement) {
        diagnostics.error(statement.line, "while lowering requires a while statement");
        return false;
    }
    if (statement.nested_statements.empty()) {
        diagnostics.error(statement.line, "while lowering requires a non-empty body");
        return false;
    }
    for (auto const& body_statement : statement.nested_statements) {
        if (body_statement.kind != syntax::StatementKind::assignment_statement) {
            diagnostics.error(
                body_statement.line,
                "lowering while body only supports mutable-local assignments"
            );
            return false;
        }
    }

    auto const block_index = next_llvm_block_index(session.state.next_block_index);
    auto const condition_block = llvm_block_name("while.condition", block_index);
    auto const body_block = llvm_block_name("while.body", block_index);
    auto const exit_block = llvm_block_name("while.exit", block_index);

    emit_llvm_branch(output, condition_block);
    emit_llvm_block_label(output, condition_block);
    session.state.current_block = condition_block;

    auto condition = lower_expression(
        statement.expression,
        "i1",
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!condition.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this while condition" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }
    emit_llvm_conditional_branch(output, condition->value, body_block, exit_block);

    emit_llvm_block_label(output, body_block);
    session.state.current_block = body_block;
    for (auto const& body_statement : statement.nested_statements) {
        if (!lower_assignment_statement(body_statement, context, session, diagnostics, output)) {
            return false;
        }
    }
    emit_llvm_branch(output, condition_block);

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return true;
}

}  // namespace orison::lowering
