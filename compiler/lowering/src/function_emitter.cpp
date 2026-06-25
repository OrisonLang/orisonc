#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/switch_plan.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/lowering/while_emitter.hpp"

#include <optional>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

using EmissionContext = LoweringEmissionContext;

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

auto lower_unit_statement_block(
    std::span<syntax::StatementSyntax const*> statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_statement_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_statement(
    syntax::StatementSyntax const& statement,
    bool is_last_statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_if_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_switch_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_repeat_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_for_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_unsafe_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_unit_defer_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_guard_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_nonvoid_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto lower_nonvoid_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow;

auto is_empty_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.text.empty() && expression.arguments.empty() && expression.nested_statements.empty() &&
           expression.left == nullptr && expression.right == nullptr && expression.alternate == nullptr;
}

auto is_concurrency_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::task ||
           expression.kind == syntax::ExpressionKind::thread ||
           (expression.kind == syntax::ExpressionKind::unary && expression.text == "await");
}

auto concurrency_expression_name(syntax::ExpressionSyntax const& expression) -> std::string {
    if (expression.kind == syntax::ExpressionKind::task) {
        return "task";
    }
    if (expression.kind == syntax::ExpressionKind::thread) {
        return "thread";
    }
    if (expression.kind == syntax::ExpressionKind::unary && expression.text == "await") {
        return "await";
    }
    return expression.text;
}

auto infer_unit_expression_type(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    auto inferred = infer_expression_type(expression, context, state);
    if (inferred.has_value()) {
        return inferred;
    }
    if (expression.kind == syntax::ExpressionKind::integer_literal) {
        return LoweredType {
            .type = "i64",
            .signedness = IntegerSignedness::signed_integer,
        };
    }
    if (expression.kind == syntax::ExpressionKind::boolean_literal) {
        return LoweredType {
            .type = "i1",
            .signedness = IntegerSignedness::not_integer,
        };
    }
    if (expression.kind == syntax::ExpressionKind::string_literal) {
        return LoweredType {
            .type = "ptr",
            .signedness = IntegerSignedness::not_integer,
        };
    }
    return std::nullopt;
}

auto is_receiver_self_source_type(std::string_view type_name) -> bool {
    return type_name == "This" || type_name == "shared.This" || type_name == "exclusive.This";
}

auto infer_unit_binding_type(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    if (!statement.annotated_type.name.empty()) {
        return lowered_type_for_source_type_name(
            render_source_type_name(statement.annotated_type),
            context.lowering
        );
    }

    auto inferred_type = infer_unit_expression_type(statement.expression, context, state);
    if (!inferred_type.has_value() || inferred_type->type.empty() || inferred_type->type == "void") {
        return std::nullopt;
    }
    return inferred_type;
}

auto lower_unit_statement_block(
    std::span<syntax::StatementSyntax const*> statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto flow = StatementFlow::falls_through;
    DeferredCleanupScope defer_scope(session.state);
    for (auto index = std::size_t {0}; index < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr) {
            return StatementFlow::failed;
        }
        if (flow == StatementFlow::terminated) {
            diagnostics.error(
                statement->line,
                "lowering does not yet support statements after a terminating Unit statement"
            );
            return StatementFlow::failed;
        }

        auto const is_last_statement = index + 1 == statements.size();
        flow = lower_unit_statement(*statement, is_last_statement, context, session, diagnostics, output);
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

auto lower_unit_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(&statement);
    }
    return lower_unit_statement_block(statement_pointers, context, session, diagnostics, output);
}

auto lower_unit_statement_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto flow = StatementFlow::falls_through;
    DeferredCleanupScope defer_scope(session.state);
    for (auto index = std::size_t {0}; index < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr) {
            return StatementFlow::failed;
        }
        if (flow == StatementFlow::terminated) {
            diagnostics.error(
                statement->line,
                "lowering does not yet support statements after a terminating Unit statement"
            );
            return StatementFlow::failed;
        }

        auto const is_last_statement = index + 1 == statements.size();
        flow = lower_unit_statement(*statement, is_last_statement, context, session, diagnostics, output);
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

auto lower_unit_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(statement.get());
    }
    return lower_unit_statement_block(statement_pointers, context, session, diagnostics, output);
}

auto lower_unit_if_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
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
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this Unit if condition" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    auto plan = plan_conditional(
        ConditionalPlanKind::if_statement,
        next_llvm_block_index(session.state.next_block_index)
    );
    auto has_else = !statement.alternate_statements.empty();
    auto binding_scope = BranchBindingScope(session.state);
    emit_llvm_conditional_branch(
        output,
        condition->value,
        plan.then_block,
        has_else ? plan.else_block : plan.merge_block
    );

    emit_llvm_block_label(output, plan.then_block);
    session.state.current_block = plan.then_block;
    auto then_flow = lower_unit_statement_block(statement.nested_statements, context, session, diagnostics, output);
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
        else_flow =
            lower_unit_statement_block(statement.alternate_statements, context, session, diagnostics, output);
        if (else_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (else_flow == StatementFlow::falls_through) {
            emit_llvm_branch(output, plan.merge_block);
        }
    }

    binding_scope.reset();
    emit_llvm_block_label(output, plan.merge_block);
    session.state.current_block = plan.merge_block;
    if (then_flow == StatementFlow::terminated && has_else && else_flow == StatementFlow::terminated) {
        return StatementFlow::terminated;
    }
    return StatementFlow::falls_through;
}

auto lower_unit_switch_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto subject_type = infer_unit_expression_type(statement.expression, context, session.state);
    if (!subject_type.has_value() ||
        (subject_type->type != "i1" && !is_integer_llvm_type(subject_type->type))) {
        diagnostics.error(statement.line, "lowering does not yet support this Unit switch subject");
        return StatementFlow::failed;
    }

    auto subject = lower_expression(
        statement.expression,
        subject_type->type,
        subject_type->signedness,
        context,
        session,
        output
    );
    if (!subject.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this Unit switch subject" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto planning = plan_switch(statement.switch_cases, *subject_type, block_index);
    if (!planning.plan.has_value()) {
        auto detail = render_control_flow_lowering_failure(ControlFlowLoweringFailure {
            .reason = planning.failure,
        });
        diagnostics.error(
            statement.line,
            "lowering does not yet support this Unit switch statement" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    auto const& plan = *planning.plan;
    auto binding_scope = BranchBindingScope(session.state);
    auto switch_targets = std::vector<LlvmSwitchTarget> {};
    switch_targets.reserve(plan.cases.size());
    for (auto const& planned_case : plan.cases) {
        if (planned_case.pattern.has_value()) {
            switch_targets.push_back(LlvmSwitchTarget {
                .value = planned_case.pattern->value,
                .block = planned_case.block,
            });
        }
    }
    emit_llvm_switch(output, subject->type, subject->value, plan.default_block, switch_targets);

    auto all_cases_terminated = true;
    for (auto const& planned_case : plan.cases) {
        binding_scope.reset();
        emit_llvm_block_label(output, planned_case.block);
        session.state.current_block = planned_case.block;

        auto case_flow = lower_unit_statement_block(
            planned_case.syntax->statements,
            context,
            session,
            diagnostics,
            output
        );
        if (case_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (case_flow == StatementFlow::falls_through) {
            emit_llvm_branch(output, plan.merge_block);
            all_cases_terminated = false;
        }
    }

    if (!plan.has_default) {
        emit_llvm_block_label(output, plan.fallback_block);
        emit_llvm_unreachable(output);
    }

    binding_scope.reset();
    emit_llvm_block_label(output, plan.merge_block);
    session.state.current_block = plan.merge_block;
    return all_cases_terminated ? StatementFlow::terminated : StatementFlow::falls_through;
}

auto lower_unit_repeat_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
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
    auto body_flow = lower_unit_statement_block(statement.nested_statements, context, session, diagnostics, output);
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

auto lower_unit_for_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
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
            auto body_flow = lower_unit_statement_block(
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
        auto inferred = infer_unit_expression_type(element, context, session.state);
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
        if (auto source_type =
                source_type_name_for_llvm_type(lowered_item->type, context.lowering)) {
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
        auto body_flow = lower_unit_statement_block(
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
            emit_llvm_branch(output, index + 1 < statement.expression.arguments.size()
                                       ? iteration_blocks[index + 1]
                                       : exit_block);
        }
    }

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return StatementFlow::falls_through;
}

auto lower_unit_unsafe_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    [[maybe_unused]] auto binding_scope = BranchBindingScope(session.state);
    auto flow = lower_unit_statement_block(statement.nested_statements, context, session, diagnostics, output);
    if (flow == StatementFlow::failed) {
        return StatementFlow::failed;
    }
    return flow;
}

auto lower_unit_defer_statement(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (!record_deferred_cleanup(statement, session, diagnostics)) {
        return StatementFlow::failed;
    }
    (void)context;
    (void)output;
    return StatementFlow::falls_through;
}

auto emit_deferred_cleanup_to_depth_impl(
    std::size_t target_depth,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (target_depth > session.state.defer_cleanup_scopes.size()) {
        diagnostics.error(1, "lowering defer cleanup depth is inconsistent with the active scope stack");
        return false;
    }

    auto scope_index = session.state.defer_cleanup_scopes.size();
    auto scope_output = std::ostringstream {};
    while (scope_index > target_depth) {
        auto const scope = session.state.defer_cleanup_scopes[scope_index - 1];
        auto cleanup_output = std::ostringstream {};
        auto const blocks = scope.blocks;
        for (auto block_index = blocks.size(); block_index-- > 0;) {
            auto const& block = blocks[block_index];
            auto block_output = std::ostringstream {};
            auto flow = lower_unit_statement_block(
                block.statements,
                context,
                session,
                diagnostics,
                block_output
            );
            if (flow == StatementFlow::failed) {
                return false;
            }
            if (flow == StatementFlow::terminated) {
                diagnostics.error(
                    block.line,
                    "lowering defer cleanup blocks currently requires them to fall through"
                );
                return false;
            }
            cleanup_output << block_output.str();
        }
        scope_output << cleanup_output.str();
        --scope_index;
    }

    output << scope_output.str();
    return true;
}

auto lower_guard_return_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (return_llvm_type == "void") {
        if (!is_empty_expression(statement.expression)) {
            diagnostics.error(statement.line, "lowering does not yet support return expressions in Unit guard failure blocks");
            return StatementFlow::failed;
        }
        if (!emit_deferred_cleanup_to_depth(0, context, session, diagnostics, output)) {
            return StatementFlow::failed;
        }
        output << "  ret void\n";
        return StatementFlow::terminated;
    }

    if (is_empty_expression(statement.expression)) {
        diagnostics.error(statement.line, "lowering does not yet support bare return statements in non-Unit guard failure blocks");
        return StatementFlow::failed;
    }

    auto lowered = lower_expression(
        statement.expression,
        return_llvm_type,
        return_signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this guard failure return" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    if (!emit_deferred_cleanup_to_depth(0, context, session, diagnostics, output)) {
        return StatementFlow::failed;
    }
    output << "  ret " << lowered->type << " " << lowered->value << "\n";
    return StatementFlow::terminated;
}

auto lower_guard_statement_block(
    std::span<syntax::StatementSyntax const*> statements,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto flow = StatementFlow::falls_through;
    DeferredCleanupScope defer_scope(session.state);
    for (auto index = std::size_t {0}; index < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr) {
            return StatementFlow::failed;
        }
        if (flow == StatementFlow::terminated) {
            diagnostics.error(
                statement->line,
                "lowering does not yet support statements after a terminating guard failure statement"
            );
            return StatementFlow::failed;
        }

        auto const is_last_statement = index + 1 == statements.size();
        if (statement->kind == syntax::StatementKind::return_statement) {
            flow = lower_guard_return_statement(
                *statement,
                return_llvm_type,
                return_signedness,
                context,
                session,
                diagnostics,
                output
            );
        } else if (statement->kind == syntax::StatementKind::if_statement) {
            flow = lower_nonvoid_if_statement(
                *statement,
                return_llvm_type,
                return_signedness,
                context,
                session,
                diagnostics,
                output
            );
        } else if (statement->kind == syntax::StatementKind::switch_statement) {
            flow = lower_nonvoid_switch_statement(
                *statement,
                return_llvm_type,
                return_signedness,
                context,
                session,
                diagnostics,
                output
            );
        } else if (statement->kind == syntax::StatementKind::guard_statement) {
            flow = lower_guard_statement(
                *statement,
                return_llvm_type,
                return_signedness,
                context,
                session,
                diagnostics,
                output
            );
        } else {
            flow = lower_unit_statement(*statement, is_last_statement, context, session, diagnostics, output);
        }

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

auto lower_guard_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(&statement);
    }
    return lower_guard_statement_block(
        statement_pointers,
        return_llvm_type,
        return_signedness,
        context,
        session,
        diagnostics,
        output
    );
}

auto lower_guard_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(statement.get());
    }
    return lower_guard_statement_block(
        statement_pointers,
        return_llvm_type,
        return_signedness,
        context,
        session,
        diagnostics,
        output
    );
}

auto lower_guard_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
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
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this guard condition" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    auto const block_index = next_llvm_block_index(session.state.next_block_index);
    auto const failure_block = llvm_block_name("guard.failure", block_index);
    auto const merge_block = llvm_block_name("guard.merge", block_index);
    emit_llvm_conditional_branch(output, condition->value, merge_block, failure_block);

    emit_llvm_block_label(output, failure_block);
    session.state.current_block = failure_block;
    [[maybe_unused]] auto failure_scope = BranchBindingScope(session.state);
    auto failure_flow = lower_guard_statement_block(
        statement.nested_statements,
        return_llvm_type,
        return_signedness,
        context,
        session,
        diagnostics,
        output
    );
    if (failure_flow == StatementFlow::failed) {
        return StatementFlow::failed;
    }
    if (failure_flow == StatementFlow::falls_through) {
        emit_llvm_branch(output, merge_block);
    }

    failure_scope.reset();
    emit_llvm_block_label(output, merge_block);
    session.state.current_block = merge_block;
    return StatementFlow::falls_through;
}

auto lower_nonvoid_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
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
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this non-void if condition" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    auto plan = plan_conditional(
        ConditionalPlanKind::if_statement,
        next_llvm_block_index(session.state.next_block_index)
    );
    auto has_else = !statement.alternate_statements.empty();
    auto binding_scope = BranchBindingScope(session.state);
    emit_llvm_conditional_branch(
        output,
        condition->value,
        plan.then_block,
        has_else ? plan.else_block : plan.merge_block
    );

    emit_llvm_block_label(output, plan.then_block);
    session.state.current_block = plan.then_block;
    auto then_flow = lower_guard_statement_block(
        statement.nested_statements,
        return_llvm_type,
        return_signedness,
        context,
        session,
        diagnostics,
        output
    );
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
        else_flow = lower_guard_statement_block(
            statement.alternate_statements,
            return_llvm_type,
            return_signedness,
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

    binding_scope.reset();
    emit_llvm_block_label(output, plan.merge_block);
    session.state.current_block = plan.merge_block;
    if (then_flow == StatementFlow::terminated && has_else && else_flow == StatementFlow::terminated) {
        return StatementFlow::terminated;
    }
    return StatementFlow::falls_through;
}

auto lower_nonvoid_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view return_llvm_type,
    IntegerSignedness return_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto subject_type = infer_unit_expression_type(statement.expression, context, session.state);
    if (!subject_type.has_value() ||
        (subject_type->type != "i1" && !is_integer_llvm_type(subject_type->type))) {
        diagnostics.error(statement.line, "lowering does not yet support this non-void switch subject");
        return StatementFlow::failed;
    }

    auto subject = lower_expression(
        statement.expression,
        subject_type->type,
        subject_type->signedness,
        context,
        session,
        output
    );
    if (!subject.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this non-void switch subject" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto planning = plan_switch(statement.switch_cases, *subject_type, block_index);
    if (!planning.plan.has_value()) {
        auto detail = render_control_flow_lowering_failure(ControlFlowLoweringFailure {
            .reason = planning.failure,
        });
        diagnostics.error(
            statement.line,
            "lowering does not yet support this non-void switch statement" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    auto const& plan = *planning.plan;
    auto binding_scope = BranchBindingScope(session.state);
    auto switch_targets = std::vector<LlvmSwitchTarget> {};
    switch_targets.reserve(plan.cases.size());
    for (auto const& planned_case : plan.cases) {
        if (planned_case.pattern.has_value()) {
            switch_targets.push_back(LlvmSwitchTarget {
                .value = planned_case.pattern->value,
                .block = planned_case.block,
            });
        }
    }
    emit_llvm_switch(output, subject->type, subject->value, plan.default_block, switch_targets);

    auto all_cases_terminated = true;
    for (auto const& planned_case : plan.cases) {
        binding_scope.reset();
        emit_llvm_block_label(output, planned_case.block);
        session.state.current_block = planned_case.block;

        auto case_flow = lower_guard_statement_block(
            planned_case.syntax->statements,
            return_llvm_type,
            return_signedness,
            context,
            session,
            diagnostics,
            output
        );
        if (case_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (case_flow == StatementFlow::falls_through) {
            emit_llvm_branch(output, plan.merge_block);
            all_cases_terminated = false;
        }
    }

    if (!plan.has_default) {
        emit_llvm_block_label(output, plan.fallback_block);
        emit_llvm_unreachable(output);
    }

    binding_scope.reset();
    emit_llvm_block_label(output, plan.merge_block);
    session.state.current_block = plan.merge_block;
    if (all_cases_terminated) {
        return StatementFlow::terminated;
    }
    return StatementFlow::falls_through;
}

auto lower_unit_statement(
    syntax::StatementSyntax const& statement,
    bool is_last_statement,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (statement.kind == syntax::StatementKind::let_binding) {
        auto type = infer_unit_binding_type(statement, context, session.state);
        if (!type.has_value()) {
            diagnostics.error(statement.line, "lowering does not yet support this Unit let binding");
            return StatementFlow::failed;
        }
        if (!lower_let_statement(
                statement,
                type->type,
                type->signedness,
                context,
                session,
                diagnostics,
                output
            )) {
            return StatementFlow::failed;
        }
        return StatementFlow::falls_through;
    }
    if (statement.kind == syntax::StatementKind::var_binding) {
        auto type = infer_unit_binding_type(statement, context, session.state);
        if (!type.has_value()) {
            diagnostics.error(statement.line, "lowering does not yet support this Unit var binding");
            return StatementFlow::failed;
        }
        if (!lower_var_statement(
                statement,
                type->type,
                type->signedness,
                context,
                session,
                diagnostics,
                output
            )) {
            return StatementFlow::failed;
        }
        return StatementFlow::falls_through;
    }
    if (statement.kind == syntax::StatementKind::assignment_statement) {
        if (!lower_assignment_statement(statement, context, session, diagnostics, output)) {
            return StatementFlow::failed;
        }
        return StatementFlow::falls_through;
    }
    if (statement.kind == syntax::StatementKind::return_statement) {
        if (!is_empty_expression(statement.expression)) {
            diagnostics.error(statement.line, "lowering does not yet support return expressions in Unit functions");
            return StatementFlow::failed;
        }
        if (!emit_deferred_cleanup_to_depth(0, context, session, diagnostics, output)) {
            return StatementFlow::failed;
        }
        output << "  ret void\n";
        return StatementFlow::terminated;
    }
    if (statement.kind == syntax::StatementKind::break_statement ||
        statement.kind == syntax::StatementKind::continue_statement) {
        if (!lower_loop_control_statement(statement, context, session, diagnostics, output)) {
            return StatementFlow::failed;
        }
        return StatementFlow::terminated;
    }
    if (statement.kind == syntax::StatementKind::switch_statement) {
        return lower_unit_switch_statement(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::guard_statement) {
        return lower_guard_statement(statement, "void", IntegerSignedness::not_integer, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_unit_if_statement(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::while_statement) {
        if (!lower_while_statement(statement, context, session, diagnostics, output)) {
            return StatementFlow::failed;
        }
        return StatementFlow::falls_through;
    }
    if (statement.kind == syntax::StatementKind::repeat_statement) {
        return lower_unit_repeat_statement(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::for_statement) {
        return lower_unit_for_statement(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::unsafe_statement) {
        return lower_unit_unsafe_statement(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::defer_statement) {
        (void)is_last_statement;
        return lower_unit_defer_statement(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::expression_statement &&
        statement.expression.kind == syntax::ExpressionKind::call) {
        if (!lower_call_statement(statement, context, session, diagnostics, output)) {
            return StatementFlow::failed;
        }
        return StatementFlow::falls_through;
    }

    diagnostics.error(statement.line, "lowering does not yet support this statement");
    return StatementFlow::failed;
}

void emit_function_body(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature const& signature,
    EmissionContext const& context,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) {
    if (function.is_async) {
        diagnostics.error(function.line, "lowering does not yet support async functions");
        return;
    }
    if (!function.generic_parameters.empty()) {
        diagnostics.error(function.line, "lowering does not yet support generic functions");
        return;
    }
    if (signature.return_type.empty()) {
        diagnostics.error(function.line, "lowering does not yet support this function return type");
        return;
    }
    if (signature.parameter_types.size() != function.parameters.size() ||
        signature.parameter_signedness.size() != function.parameters.size()) {
        diagnostics.error(function.line, "lowering does not yet support this function parameter type");
        return;
    }
    for (auto const& parameter_type : signature.parameter_types) {
        if (parameter_type.empty() || parameter_type == "void") {
            diagnostics.error(function.line, "lowering does not yet support this function parameter type");
            return;
        }
    }

    output << "define " << signature.return_type << " @" << signature.symbol_name << "(";
    for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << signature.parameter_types[index] << " "
               << llvm_local_value_name(function.parameters[index].name);
    }
    output << ") {\n";
    output << "entry:\n";

    auto state = FunctionLoweringState {};
    auto failures = LoweringFailures {};
    auto session = FunctionLoweringSession {
        .state = state,
        .failures = failures,
    };
    for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
        state.local_name_counts[function.parameters[index].name] = 1;
        state.immutable_bindings.emplace(function.parameters[index].name, LoweredExpression {
            .type = signature.parameter_types[index],
            .value = llvm_local_value_name(function.parameters[index].name),
            .signedness = signature.parameter_signedness[index],
        });
        auto source_type_name = render_source_type_name(function.parameters[index].type);
        if (is_receiver_self_source_type(source_type_name)) {
            if (auto concrete_source_type =
                    source_type_name_for_llvm_type(signature.parameter_types[index], context.lowering)) {
                source_type_name = std::move(*concrete_source_type);
            }
        }
        state.source_type_names.emplace(function.parameters[index].name, std::move(source_type_name));
        bind_addressable_aggregate_value(
            function.parameters[index].name,
            LoweredExpression {
                .type = signature.parameter_types[index],
                .value = llvm_local_value_name(function.parameters[index].name),
                .signedness = signature.parameter_signedness[index],
            },
            session,
            output
        );
    }
    [[maybe_unused]] auto function_scope = DeferredCleanupScope {state};

    if (signature.return_type == "void") {
        auto flow = lower_unit_statement_block(function.body_statements, context, session, diagnostics, output);
        if (flow == StatementFlow::failed) {
            return;
        }

        if (flow == StatementFlow::falls_through) {
            if (!emit_deferred_cleanup_to_depth(0, context, session, diagnostics, output)) {
                return;
            }
            output << "  ret void\n";
        }
        output << "}\n";
        return;
    }

    auto const* expression = static_cast<syntax::ExpressionSyntax const*>(nullptr);
    auto lowered_final_statement = std::optional<LoweredExpression> {};
    auto attempted_final_control_flow = false;
    auto final_statement_line = function.line;
    auto leading_statement_flow = StatementFlow::falls_through;
    auto propagate_leading_statement_flow = [&leading_statement_flow](StatementFlow flow) -> bool {
        if (flow == StatementFlow::failed) {
            return false;
        }
        if (flow == StatementFlow::terminated) {
            leading_statement_flow = StatementFlow::terminated;
        }
        return true;
    };
    for (auto index = std::size_t {0}; index < function.body_statements.size(); ++index) {
        auto const& statement = function.body_statements[index];
        auto is_last_statement = index + 1 == function.body_statements.size();
        if (leading_statement_flow == StatementFlow::terminated) {
            diagnostics.error(
                statement.line,
                "lowering does not yet support statements after a terminating non-Unit statement"
            );
            return;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::let_binding) {
            auto type = infer_unit_binding_type(statement, context, session.state);
            if (!type.has_value()) {
                if (is_concurrency_expression(statement.expression)) {
                    diagnostics.error(
                        statement.line,
                        "lowering does not yet support " +
                            concurrency_expression_name(statement.expression) + " expressions"
                    );
                } else {
                    diagnostics.error(statement.line, "lowering does not yet support this let binding");
                }
                return;
            }
            if (!lower_let_statement(
                    statement,
                    type->type,
                    type->signedness,
                    context,
                    session,
                    diagnostics,
                    output
                )) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::var_binding) {
            if (!lower_var_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    context,
                    session,
                    diagnostics,
                    output
                )) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::assignment_statement) {
            if (!lower_assignment_statement(
                    statement,
                    context,
                    session,
                    diagnostics,
                    output
                )) {
                return;
            }
            continue;
        }
        if (statement.kind == syntax::StatementKind::defer_statement) {
            if (!record_deferred_cleanup(statement, session, diagnostics)) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::while_statement) {
            if (!lower_while_statement(statement, context, session, diagnostics, output)) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::repeat_statement) {
            if (!propagate_leading_statement_flow(
                    lower_unit_repeat_statement(statement, context, session, diagnostics, output)
                )) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::for_statement) {
            if (!propagate_leading_statement_flow(
                    lower_unit_for_statement(statement, context, session, diagnostics, output)
                )) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::unsafe_statement) {
            if (!propagate_leading_statement_flow(
                    lower_unit_unsafe_statement(statement, context, session, diagnostics, output)
                )) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::expression_statement &&
            statement.expression.kind == syntax::ExpressionKind::call) {
            if (!lower_call_statement(statement, context, session, diagnostics, output)) {
                return;
            }
            continue;
        }
        if (statement.kind == syntax::StatementKind::guard_statement) {
            if (!propagate_leading_statement_flow(lower_guard_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    context,
                    session,
                    diagnostics,
                    output
                ))) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::if_statement) {
            if (!propagate_leading_statement_flow(lower_nonvoid_if_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    context,
                    session,
                    diagnostics,
                    output
                ))) {
                return;
            }
            continue;
        }
        if (!is_last_statement && statement.kind == syntax::StatementKind::switch_statement) {
            if (!propagate_leading_statement_flow(lower_nonvoid_switch_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    context,
                    session,
                    diagnostics,
                    output
                ))) {
                return;
            }
            continue;
        }

        if (is_last_statement) {
            if (statement.kind == syntax::StatementKind::if_statement ||
                statement.kind == syntax::StatementKind::switch_statement) {
                attempted_final_control_flow = true;
                final_statement_line = statement.line;
                lowered_final_statement = lower_final_control_flow_statement(
                    statement,
                    signature.return_type,
                    signature.return_signedness,
                    context,
                    session,
                    diagnostics,
                    output
                );
                break;
            }
            expression = value_expression_for(statement);
            break;
        }

        diagnostics.error(statement.line, "lowering does not yet support this statement");
        return;
    }

    if (attempted_final_control_flow && !lowered_final_statement.has_value()) {
        auto detail = render_control_flow_lowering_failure(failures.control_flow);
        diagnostics.error(
            final_statement_line,
            "lowering does not yet support this final control-flow statement" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return;
    }

    if (!lowered_final_statement.has_value() && expression == nullptr) {
        diagnostics.error(
            function.line,
            "lowering requires supported leading statements followed by a return or final expression"
        );
        return;
    }

    auto lowered = std::move(lowered_final_statement);
    if (!lowered.has_value()) {
        lowered = lower_expression(
            *expression,
            signature.return_type,
            signature.return_signedness,
            context,
            session,
            output
        );
    }
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(failures.expression);
        diagnostics.error(
            expression != nullptr ? expression->line : function.line,
            "lowering does not yet support this return expression" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return;
    }

    if (!emit_deferred_cleanup_to_depth(0, context, session, diagnostics, output)) {
        return;
    }
    output << "  ret " << lowered->type << " " << lowered->value << "\n";
    output << "}\n";
}

}  // namespace

auto emit_deferred_cleanup_to_depth(
    std::size_t target_depth,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    return emit_deferred_cleanup_to_depth_impl(target_depth, context, session, diagnostics, output);
}

auto emit_function(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature const& signature,
    LoweringContext const& lowering_context,
    StringConstantTable const& string_constants,
    diagnostics::DiagnosticBag& diagnostics
) -> std::string {
    auto output = std::ostringstream {};
    auto context = EmissionContext {
        .lowering = lowering_context,
        .string_constants = string_constants,
    };
    emit_function_body(function, signature, context, diagnostics, output);
    return output.str();
}

}  // namespace orison::lowering
