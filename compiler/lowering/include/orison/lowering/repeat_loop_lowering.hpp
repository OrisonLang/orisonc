#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace orison::lowering {

struct RepeatLoopBlockPlan {
    std::string body_block;
    std::string condition_block;
    std::string exit_block;
};

auto plan_repeat_loop_blocks(FunctionLoweringState& state) -> RepeatLoopBlockPlan;

auto repeat_loop_targets(RepeatLoopBlockPlan const& plan, std::size_t defer_cleanup_depth)
    -> LoopTargets;

template <typename LowerBody>
auto lower_repeat_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    LowerBody&& lower_body
) -> StatementFlow {
    if (statement.nested_statements.empty()) {
        diagnostics.error(statement.line, "lowering repeat statements requires a non-empty body");
        return StatementFlow::failed;
    }

    auto block_plan = plan_repeat_loop_blocks(session.state);

    emit_llvm_branch(output, block_plan.body_block);
    emit_llvm_block_label(output, block_plan.body_block);
    session.state.current_block = block_plan.body_block;

    [[maybe_unused]] auto loop_scope = LoopTargetScope {
        session.state,
        repeat_loop_targets(block_plan, session.state.defer_cleanup_scopes.size()),
    };
    [[maybe_unused]] auto body_scope = BranchBindingScope(session.state);
    auto body_flow = lower_body();
    if (body_flow == StatementFlow::failed) {
        return StatementFlow::failed;
    }
    if (body_flow == StatementFlow::falls_through) {
        emit_llvm_branch(output, block_plan.condition_block);
    }

    emit_llvm_block_label(output, block_plan.condition_block);
    session.state.current_block = block_plan.condition_block;
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
            "lowering does not yet support this repeat condition" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }
    emit_llvm_conditional_branch(
        output,
        condition->value,
        block_plan.body_block,
        block_plan.exit_block
    );

    emit_llvm_block_label(output, block_plan.exit_block);
    session.state.current_block = block_plan.exit_block;
    return StatementFlow::falls_through;
}

}  // namespace orison::lowering
