#include "orison/lowering/while_emitter.hpp"

#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/statement_emitter.hpp"

#include <string>

namespace orison::lowering {
namespace {

class LoopTargetScope {
public:
    LoopTargetScope(FunctionLoweringState& state, LoopTargets targets)
        : state_(state) {
        state_.loop_targets.push_back(std::move(targets));
    }

    ~LoopTargetScope() {
        state_.loop_targets.pop_back();
    }

    LoopTargetScope(LoopTargetScope const&) = delete;
    auto operator=(LoopTargetScope const&) -> LoopTargetScope& = delete;

private:
    FunctionLoweringState& state_;
};

}  // namespace

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
        auto const is_assignment =
            body_statement.kind == syntax::StatementKind::assignment_statement;
        auto const is_call =
            body_statement.kind == syntax::StatementKind::expression_statement &&
            body_statement.expression.kind == syntax::ExpressionKind::call;
        auto const is_loop_control =
            body_statement.kind == syntax::StatementKind::break_statement ||
            body_statement.kind == syntax::StatementKind::continue_statement;
        if (!is_assignment && !is_call && !is_loop_control) {
            diagnostics.error(
                body_statement.line,
                "lowering while body only supports mutable-local assignments, call statements, "
                "and terminal loop control"
            );
            return false;
        }
    }
    for (auto index = std::size_t {0}; index + 1 < statement.nested_statements.size(); ++index) {
        auto const kind = statement.nested_statements[index].kind;
        if (kind == syntax::StatementKind::break_statement ||
            kind == syntax::StatementKind::continue_statement) {
            diagnostics.error(
                statement.nested_statements[index].line,
                "lowering requires break or continue to end the while body"
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
    auto loop_scope = LoopTargetScope {
        session.state,
        LoopTargets {
            .break_target = exit_block,
            .continue_target = condition_block,
        },
    };
    auto body_terminated = false;
    for (auto const& body_statement : statement.nested_statements) {
        auto lowered = false;
        if (body_statement.kind == syntax::StatementKind::assignment_statement) {
            lowered = lower_assignment_statement(body_statement, context, session, diagnostics, output);
        } else if (body_statement.kind == syntax::StatementKind::expression_statement) {
            lowered = lower_call_statement(body_statement, context, session, diagnostics, output);
        } else {
            lowered = lower_loop_control_statement(body_statement, session, diagnostics, output);
            body_terminated = lowered;
        }
        if (!lowered) {
            return false;
        }
    }
    if (!body_terminated) {
        emit_llvm_branch(output, condition_block);
    }

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return true;
}

}  // namespace orison::lowering
