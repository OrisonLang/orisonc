#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/syntax/module_parser.hpp"

#include <sstream>
#include <string>

namespace orison::lowering {

template <typename LowerBody>
auto lower_while_loop_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    LowerBody&& lower_body
) -> StatementFlow {
    if (statement.kind != syntax::StatementKind::while_statement) {
        diagnostics.error(statement.line, "while lowering requires a while statement");
        return StatementFlow::failed;
    }
    if (statement.nested_statements.empty()) {
        diagnostics.error(statement.line, "while lowering requires a non-empty body");
        return StatementFlow::failed;
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
        return StatementFlow::failed;
    }
    emit_llvm_conditional_branch(output, condition->value, body_block, exit_block);

    emit_llvm_block_label(output, body_block);
    session.state.current_block = body_block;
    auto loop_scope = LoopTargetScope {
        session.state,
        LoopTargets {
            .break_target = exit_block,
            .continue_target = condition_block,
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        },
    };
    auto body_scope = BranchBindingScope(session.state);
    auto body_flow = lower_body();
    if (body_flow == StatementFlow::failed) {
        return StatementFlow::failed;
    }
    if (body_flow == StatementFlow::falls_through) {
        emit_llvm_branch(output, condition_block);
    }

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return StatementFlow::falls_through;
}

}  // namespace orison::lowering
