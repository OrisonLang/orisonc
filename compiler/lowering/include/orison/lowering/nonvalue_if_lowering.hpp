#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/syntax/module_parser.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace orison::lowering {

template <typename ThenLowerer, typename ElseLowerer>
auto lower_nonvalue_if_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    std::string_view unsupported_condition_diagnostic,
    bool emit_merge_for_fully_terminated,
    ThenLowerer lower_then,
    ElseLowerer lower_else
) -> StatementFlow {
    auto condition = lower_expression(
        statement.expression,
        "i1",
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!condition.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(unsupported_condition_diagnostic, session.failures.expression)
        );
        return StatementFlow::failed;
    }

    auto plan = plan_conditional(
        ConditionalPlanKind::if_statement,
        next_llvm_block_index(session.state.next_block_index)
    );
    auto const has_else = !statement.alternate_statements.empty();
    auto binding_scope = BranchBindingScope(session.state);
    auto fallthrough_consumed_bindings = std::vector<std::unordered_set<std::string>> {};
    emit_llvm_conditional_branch(
        output,
        condition->value,
        plan.then_block,
        has_else ? plan.else_block : plan.merge_block
    );

    emit_llvm_block_label(output, plan.then_block);
    session.state.current_block = plan.then_block;
    auto then_flow = lower_then();
    if (then_flow == StatementFlow::failed) {
        return StatementFlow::failed;
    }
    if (then_flow == StatementFlow::falls_through) {
        fallthrough_consumed_bindings.push_back(session.state.consumed_owned_dynamic_array_bindings);
        emit_llvm_branch(output, plan.merge_block);
    }

    auto else_flow = StatementFlow::falls_through;
    if (has_else) {
        binding_scope.reset();
        emit_llvm_block_label(output, plan.else_block);
        session.state.current_block = plan.else_block;
        else_flow = lower_else();
        if (else_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (else_flow == StatementFlow::falls_through) {
            fallthrough_consumed_bindings.push_back(session.state.consumed_owned_dynamic_array_bindings);
            emit_llvm_branch(output, plan.merge_block);
        }
    } else {
        fallthrough_consumed_bindings.push_back(binding_scope.saved_consumed_owned_dynamic_array_bindings());
    }

    auto const fully_terminated =
        then_flow == StatementFlow::terminated && has_else && else_flow == StatementFlow::terminated;
    if (fully_terminated && !emit_merge_for_fully_terminated) {
        return StatementFlow::terminated;
    }

    auto merged_consumed_bindings = merge_consumed_owned_dynamic_array_bindings(fallthrough_consumed_bindings);
    if (!merged_consumed_bindings.has_value()) {
        diagnostics.error(
            statement.line,
            "if branch ownership mismatch: owned DynamicArray moves must match across all continuing branches"
        );
        return StatementFlow::failed;
    }
    binding_scope.commit_consumed_owned_dynamic_array_bindings(std::move(*merged_consumed_bindings));
    emit_llvm_block_label(output, plan.merge_block);
    session.state.current_block = plan.merge_block;
    return fully_terminated ? StatementFlow::terminated : StatementFlow::falls_through;
}

}  // namespace orison::lowering
