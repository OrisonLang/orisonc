#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace orison::lowering {

struct ForLoopBlockPlan {
    std::string exit_block;
    std::vector<std::string> iteration_blocks;
};

auto plan_for_loop_blocks(FunctionLoweringState& state, std::size_t iteration_count)
    -> ForLoopBlockPlan;

auto next_for_iteration_target(ForLoopBlockPlan const& plan, std::size_t index)
    -> std::string const&;

template <typename LowerBody>
auto lower_fixed_array_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    LowerBody&& lower_body
) -> StatementFlow {
    auto source_type_name =
        source_type_name_for_expression(statement.expression, context.lowering, session.state);
    if (!source_type_name.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires an array literal or fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto iterable_type = lowered_type_for_source_type_name(*source_type_name, context.lowering);
    if (!iterable_type.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires a fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto array_type = parse_llvm_array_type(iterable_type->type);
    if (!array_type.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires a fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto element_source_type_name = array_element_source_type_name(*source_type_name);
    if (!element_source_type_name.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires a named fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto element_type = lowered_type_for_source_type_name(*element_source_type_name, context.lowering);
    if (!element_type.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires a fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto lowered_iterable = lower_expression(
        statement.expression,
        iterable_type->type,
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!lowered_iterable.has_value()) {
        diagnostics.error(
            statement.expression.line,
            append_expression_lowering_failure(
                "lowering for statements currently requires a fixed-size array iterable",
                session.failures.expression
            )
        );
        return StatementFlow::failed;
    }

    auto block_plan = plan_for_loop_blocks(session.state, array_type->length);
    if (block_plan.iteration_blocks.empty()) {
        emit_llvm_branch(output, block_plan.exit_block);
        emit_llvm_block_label(output, block_plan.exit_block);
        session.state.current_block = block_plan.exit_block;
        return StatementFlow::falls_through;
    }

    auto loop_scope = BranchBindingScope(session.state);
    emit_llvm_branch(output, block_plan.iteration_blocks.front());

    for (auto index = std::size_t {0}; index < array_type->length; ++index) {
        loop_scope.reset();
        emit_llvm_block_label(output, block_plan.iteration_blocks[index]);
        session.state.current_block = block_plan.iteration_blocks[index];

        auto item_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << item_name << " = extractvalue " << lowered_iterable->type << " "
               << lowered_iterable->value << ", " << index << "\n";
        session.state.immutable_bindings[statement.name] = LoweredExpression {
            .type = element_type->type,
            .value = item_name,
            .signedness = element_type->signedness,
        };
        session.state.source_type_names[statement.name] = *element_source_type_name;
        bind_addressable_aggregate_value(
            statement.name,
            session.state.immutable_bindings.at(statement.name),
            session,
            output
        );

        auto loop_targets = LoopTargets {
            .break_target = block_plan.exit_block,
            .continue_target = next_for_iteration_target(block_plan, index),
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        };
        [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
        auto body_flow = lower_body();
        if (body_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (body_flow == StatementFlow::falls_through) {
            emit_llvm_branch(output, next_for_iteration_target(block_plan, index));
        }
    }

    emit_llvm_block_label(output, block_plan.exit_block);
    session.state.current_block = block_plan.exit_block;
    return StatementFlow::falls_through;
}

template <typename InferElementType, typename LowerBody>
auto lower_array_literal_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    InferElementType&& infer_element_type,
    LowerBody&& lower_body
) -> StatementFlow {
    auto element_type = std::optional<LoweredType> {};
    for (auto const& element : statement.expression.arguments) {
        auto inferred = infer_element_type(element, context, session.state);
        if (!inferred.has_value()) {
            continue;
        }
        if (!element_type.has_value()) {
            element_type = *inferred;
            continue;
        }
        if (element_type->type != inferred->type || element_type->signedness != inferred->signedness) {
            diagnostics.error(statement.line, "lowering for statements currently requires a uniform iterable type");
            return StatementFlow::failed;
        }
    }
    if (!element_type.has_value()) {
        element_type = LoweredType {
            .type = "i64",
            .signedness = IntegerSignedness::signed_integer,
        };
    }

    auto block_plan = plan_for_loop_blocks(session.state, statement.expression.arguments.size());
    if (block_plan.iteration_blocks.empty()) {
        emit_llvm_branch(output, block_plan.exit_block);
        emit_llvm_block_label(output, block_plan.exit_block);
        session.state.current_block = block_plan.exit_block;
        return StatementFlow::falls_through;
    }

    auto loop_scope = BranchBindingScope(session.state);
    emit_llvm_branch(output, block_plan.iteration_blocks.front());

    for (auto index = std::size_t {0}; index < statement.expression.arguments.size(); ++index) {
        loop_scope.reset();
        emit_llvm_block_label(output, block_plan.iteration_blocks[index]);
        session.state.current_block = block_plan.iteration_blocks[index];

        auto lowered_item = lower_expression(
            statement.expression.arguments[index],
            element_type->type,
            element_type->signedness,
            context,
            session,
            output
        );
        if (!lowered_item.has_value()) {
            diagnostics.error(
                statement.expression.arguments[index].line,
                append_expression_lowering_failure(
                    "lowering array-literal for statements requires an explicit Array<T, N> "
                    "source type when element type cannot be inferred; add a typed local "
                    "binding or cast the iterable with 'as Array<T, N>'",
                    session.failures.expression
                )
            );
            return StatementFlow::failed;
        }

        session.state.immutable_bindings[statement.name] = LoweredExpression {
            .type = lowered_item->type,
            .value = lowered_item->value,
            .signedness = lowered_item->signedness,
        };
        if (auto source_type = source_type_name_for_llvm_type(lowered_item->type, context.lowering)) {
            session.state.source_type_names[statement.name] = std::move(*source_type);
        }
        bind_addressable_aggregate_value(
            statement.name,
            session.state.immutable_bindings.at(statement.name),
            session,
            output
        );

        auto loop_targets = LoopTargets {
            .break_target = block_plan.exit_block,
            .continue_target = next_for_iteration_target(block_plan, index),
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        };
        [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
        auto body_flow = lower_body();
        if (body_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (body_flow == StatementFlow::falls_through) {
            emit_llvm_branch(output, next_for_iteration_target(block_plan, index));
        }
    }

    emit_llvm_block_label(output, block_plan.exit_block);
    session.state.current_block = block_plan.exit_block;
    return StatementFlow::falls_through;
}

}  // namespace orison::lowering
