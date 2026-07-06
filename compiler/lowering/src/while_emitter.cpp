#include "orison/lowering/while_emitter.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

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
    auto binding_scope = BranchBindingScope(session.state);
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
        binding_scope.reset();
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

    binding_scope.reset();
    emit_llvm_block_label(output, plan.merge_block);
    session.state.current_block = plan.merge_block;
    return StatementFlow::falls_through;
}

auto lower_while_body_repeat(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (statement.nested_statements.empty()) {
        diagnostics.error(statement.line, "lowering repeat statements requires a non-empty body");
        return StatementFlow::failed;
    }

    auto const block_index = next_llvm_block_index(session.state.next_block_index);
    auto const body_block = llvm_block_name("repeat.body", block_index);
    auto const condition_block = llvm_block_name("repeat.condition", block_index);
    auto const exit_block = llvm_block_name("repeat.exit", block_index);

    emit_llvm_branch(output, body_block);
    emit_llvm_block_label(output, body_block);
    session.state.current_block = body_block;

    [[maybe_unused]] auto loop_scope = LoopTargetScope {
        session.state,
        LoopTargets {
            .break_target = exit_block,
            .continue_target = condition_block,
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        },
    };
    [[maybe_unused]] auto body_scope = BranchBindingScope(session.state);
    auto body_flow = lower_while_body_block(statement.nested_statements, context, session, diagnostics, output);
    if (body_flow == StatementFlow::failed) {
        return StatementFlow::failed;
    }
    if (body_flow == StatementFlow::falls_through) {
        emit_llvm_branch(output, condition_block);
    }

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
            "lowering does not yet support this repeat condition" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }
    emit_llvm_conditional_branch(output, condition->value, body_block, exit_block);

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return StatementFlow::falls_through;
}

auto lower_while_body_for(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (statement.expression.kind != syntax::ExpressionKind::array_literal) {
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
            auto detail = render_expression_lowering_failure(session.failures.expression);
            diagnostics.error(
                statement.expression.line,
                "lowering for statements currently requires a fixed-size array iterable" +
                    (detail.empty() ? std::string {} : ": " + detail)
            );
            return StatementFlow::failed;
        }

        auto const block_index = next_llvm_block_index(session.state.next_block_index);
        auto const exit_block = llvm_block_name("for.exit", block_index);
        auto iteration_blocks = std::vector<std::string> {};
        iteration_blocks.reserve(array_type->length);
        for (auto index = std::size_t {0}; index < array_type->length; ++index) {
            iteration_blocks.push_back(llvm_block_name("for.iteration", block_index, index));
        }

        if (iteration_blocks.empty()) {
            emit_llvm_branch(output, exit_block);
            emit_llvm_block_label(output, exit_block);
            session.state.current_block = exit_block;
            return StatementFlow::falls_through;
        }

        auto loop_scope = BranchBindingScope(session.state);
        emit_llvm_branch(output, iteration_blocks.front());

        for (auto index = std::size_t {0}; index < array_type->length; ++index) {
            loop_scope.reset();
            emit_llvm_block_label(output, iteration_blocks[index]);
            session.state.current_block = iteration_blocks[index];

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
                .break_target = exit_block,
                .continue_target = index + 1 < array_type->length ? iteration_blocks[index + 1] : exit_block,
                .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
            };
            [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
            auto body_flow = lower_while_body_block(
                statement.nested_statements,
                context,
                session,
                diagnostics,
                output
            );
            if (body_flow == StatementFlow::failed) {
                return StatementFlow::failed;
            }
            if (body_flow == StatementFlow::falls_through) {
                emit_llvm_branch(output, index + 1 < array_type->length ? iteration_blocks[index + 1]
                                                                        : exit_block);
            }
        }

        emit_llvm_block_label(output, exit_block);
        session.state.current_block = exit_block;
        return StatementFlow::falls_through;
    }

    auto element_type = std::optional<LoweredType> {};
    for (auto const& element : statement.expression.arguments) {
        auto inferred = infer_expression_type(element, context, session.state);
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

    auto const block_index = next_llvm_block_index(session.state.next_block_index);
    auto const exit_block = llvm_block_name("for.exit", block_index);
    auto iteration_blocks = std::vector<std::string> {};
    iteration_blocks.reserve(statement.expression.arguments.size());
    for (auto index = std::size_t {0}; index < statement.expression.arguments.size(); ++index) {
        iteration_blocks.push_back(llvm_block_name("for.iteration", block_index, index));
    }

    if (iteration_blocks.empty()) {
        emit_llvm_branch(output, exit_block);
        emit_llvm_block_label(output, exit_block);
        session.state.current_block = exit_block;
        return StatementFlow::falls_through;
    }

    auto loop_scope = BranchBindingScope(session.state);
    emit_llvm_branch(output, iteration_blocks.front());

    for (auto index = std::size_t {0}; index < statement.expression.arguments.size(); ++index) {
        loop_scope.reset();
        emit_llvm_block_label(output, iteration_blocks[index]);
        session.state.current_block = iteration_blocks[index];

        auto lowered_item = lower_expression(
            statement.expression.arguments[index],
            element_type->type,
            element_type->signedness,
            context,
            session,
            output
        );
        if (!lowered_item.has_value()) {
            auto detail = render_expression_lowering_failure(session.failures.expression);
            diagnostics.error(
                statement.expression.arguments[index].line,
                "lowering does not yet support this for iterable element" +
                    (detail.empty() ? std::string {} : ": " + detail)
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
            .break_target = exit_block,
            .continue_target = index + 1 < statement.expression.arguments.size()
                ? iteration_blocks[index + 1]
                : exit_block,
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        };
        [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
        auto body_flow = lower_while_body_block(
            statement.nested_statements,
            context,
            session,
            diagnostics,
            output
        );
        if (body_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (body_flow == StatementFlow::falls_through) {
            emit_llvm_branch(
                output,
                index + 1 < statement.expression.arguments.size()
                    ? iteration_blocks[index + 1]
                    : exit_block
            );
        }
    }

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return StatementFlow::falls_through;
}

auto lower_while_body_unsafe(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    [[maybe_unused]] auto binding_scope = BranchBindingScope(session.state);
    auto flow = lower_while_body_block(statement.nested_statements, context, session, diagnostics, output);
    return flow;
}

auto inferred_loop_binding_type(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    if (!statement.annotated_type.name.empty()) {
        auto type = llvm_type_for(statement.annotated_type);
        if (!type.has_value() || *type == "void") {
            return std::nullopt;
        }
        return LoweredType {
            .type = std::string(*type),
            .signedness = integer_signedness_for(statement.annotated_type),
        };
    }
    return infer_expression_type(statement.expression, context, state);
}

auto lower_while_body_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (statement.kind == syntax::StatementKind::let_binding) {
        auto type = inferred_loop_binding_type(statement, context, session.state);
        if (!type.has_value()) {
            diagnostics.error(statement.line, "lowering does not yet support this while let type");
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
        auto type = inferred_loop_binding_type(statement, context, session.state);
        if (!type.has_value()) {
            diagnostics.error(statement.line, "lowering does not yet support this while var type");
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
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_while_body_if(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::while_statement) {
        return lower_while_statement(statement, context, session, diagnostics, output)
            ? StatementFlow::falls_through
            : StatementFlow::failed;
    }
    if (statement.kind == syntax::StatementKind::repeat_statement) {
        return lower_while_body_repeat(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::for_statement) {
        return lower_while_body_for(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::unsafe_statement) {
        return lower_while_body_unsafe(statement, context, session, diagnostics, output);
    }

    diagnostics.error(
        statement.line,
        "lowering while body only supports local bindings, mutable-local assignments, "
        "call statements, loop control, nested if statements, nested while/repeat/for statements, and unsafe blocks"
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
    DeferredCleanupScope defer_scope(session.state);
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
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        },
    };
    auto body_scope = BranchBindingScope(session.state);
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
