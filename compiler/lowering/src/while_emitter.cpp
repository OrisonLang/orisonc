#include "orison/lowering/while_emitter.hpp"

#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/statement_emitter.hpp"

#include <string>

namespace orison::lowering {
namespace {

enum class StatementFlow {
    falls_through,
    terminated,
    failed,
};

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

auto lower_while_body_block(
    std::vector<syntax::StatementSyntax> const& statements,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_while_body_if(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (statement.nested_statements.empty()) {
        diagnostics.error(statement.line, "lowering while-body if requires a non-empty then arm");
        return StatementFlow::failed;
    }

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
            "lowering does not yet support this while-body if condition" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    auto plan = plan_conditional(
        ConditionalPlanKind::if_statement,
        next_llvm_block_index(session.state.next_block_index)
    );
    auto const has_else = !statement.alternate_statements.empty();
    emit_llvm_conditional_branch(
        output,
        condition->value,
        plan.then_block,
        has_else ? plan.else_block : plan.merge_block
    );

    emit_llvm_block_label(output, plan.then_block);
    session.state.current_block = plan.then_block;
    auto then_flow =
        lower_while_body_block(statement.nested_statements, context, session, diagnostics, output);
    if (then_flow == StatementFlow::failed) {
        return StatementFlow::failed;
    }
    if (then_flow == StatementFlow::falls_through) {
        emit_llvm_branch(output, plan.merge_block);
    }

    auto else_flow = StatementFlow::falls_through;
    if (has_else) {
        emit_llvm_block_label(output, plan.else_block);
        session.state.current_block = plan.else_block;
        else_flow = lower_while_body_block(
            statement.alternate_statements,
            context,
            session,
            diagnostics,
            output
        );
        if (else_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (else_flow == StatementFlow::falls_through) {
            emit_llvm_branch(output, plan.merge_block);
        }
    }

    if (then_flow == StatementFlow::terminated && has_else &&
        else_flow == StatementFlow::terminated) {
        return StatementFlow::terminated;
    }

    emit_llvm_block_label(output, plan.merge_block);
    session.state.current_block = plan.merge_block;
    return StatementFlow::falls_through;
}

auto lower_while_body_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
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
    if (statement.kind == syntax::StatementKind::break_statement ||
        statement.kind == syntax::StatementKind::continue_statement) {
        return lower_loop_control_statement(statement, session, diagnostics, output)
            ? StatementFlow::terminated
            : StatementFlow::failed;
    }
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_while_body_if(statement, context, session, diagnostics, output);
    }

    diagnostics.error(
        statement.line,
        "lowering while body only supports mutable-local assignments, call statements, "
        "loop control, and nested if statements"
    );
    return StatementFlow::failed;
}

auto lower_while_body_block(
    std::vector<syntax::StatementSyntax> const& statements,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto flow = StatementFlow::falls_through;
    for (auto const& statement : statements) {
        if (flow == StatementFlow::terminated) {
            diagnostics.error(
                statement.line,
                "lowering does not support statements after terminating loop control"
            );
            return StatementFlow::failed;
        }
        flow = lower_while_body_statement(statement, context, session, diagnostics, output);
        if (flow == StatementFlow::failed) {
            return flow;
        }
    }
    return flow;
}

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
    auto body_flow =
        lower_while_body_block(statement.nested_statements, context, session, diagnostics, output);
    if (body_flow == StatementFlow::failed) {
        return false;
    }
    if (body_flow == StatementFlow::falls_through) {
        emit_llvm_branch(output, condition_block);
    }

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return true;
}

}  // namespace orison::lowering
