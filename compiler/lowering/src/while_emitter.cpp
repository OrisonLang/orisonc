#include "orison/lowering/while_emitter.hpp"

#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/for_loop_lowering.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/maybe_switch_lowering.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/repeat_loop_lowering.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/statement_body_lowering.hpp"
#include "orison/lowering/switch_plan.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/lowering/unsafe_block_lowering.hpp"
#include "orison/lowering/unit_deferred_cleanup.hpp"
#include "orison/lowering/while_loop_lowering.hpp"

#include <memory>
#include <optional>
#include <span>
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

auto lower_while_body_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
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
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this while-body if condition",
                session.failures.expression
            )
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

auto lower_while_body_switch(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto subject_type = infer_expression_type(statement.expression, context, session.state);
    auto subject_source_type = source_type_name_for_expression(
        statement.expression,
        context.lowering,
        session.state
    );
    if (!subject_type.has_value() || !is_supported_switch_subject(*subject_type, context, subject_source_type)) {
        diagnostics.error(statement.line, "lowering does not yet support this while-body switch subject");
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
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(
                "lowering does not yet support this while-body switch subject",
                session.failures.expression
            )
        );
        return StatementFlow::failed;
    }
    auto original_subject = *subject;
    auto switch_subject = switch_subject_for_emit(
        std::move(*subject),
        context,
        session,
        output,
        subject_source_type
    );

    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto planning = plan_switch(
        statement.switch_cases,
        *subject_type,
        context.lowering,
        subject_source_type,
        block_index
    );
    if (!planning.plan.has_value()) {
        diagnostics.error(
            statement.line,
            append_control_flow_lowering_failure(
                "lowering does not yet support this while-body switch statement",
                planning.failure
            )
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
    emit_llvm_switch(output, switch_subject.type, switch_subject.value, plan.default_block, switch_targets);

    auto all_cases_terminated = true;
    for (auto const& planned_case : plan.cases) {
        binding_scope.reset();
        emit_llvm_block_label(output, planned_case.block);
        session.state.current_block = planned_case.block;
        bind_switch_payload(planned_case, original_subject, context, session, output, subject_source_type);

        auto case_flow = lower_while_body_block(
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

    if (all_cases_terminated) {
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
    return lower_repeat_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        [&]() {
            return lower_while_body_block(statement.nested_statements, context, session, diagnostics, output);
        }
    );
}

auto lower_while_body_for(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    if (statement.expression.kind != syntax::ExpressionKind::array_literal) {
        return lower_fixed_array_for_statement(
            statement,
            context,
            session,
            diagnostics,
            output,
            [&]() {
                return lower_while_body_block(
                    statement.nested_statements,
                    context,
                    session,
                    diagnostics,
                    output
                );
            }
        );
    }

    return lower_array_literal_for_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        infer_expression_type,
        [&]() {
            return lower_while_body_block(
                statement.nested_statements,
                context,
                session,
                diagnostics,
                output
            );
        }
    );
}

auto lower_while_body_unsafe(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    return lower_unsafe_block(session, [&]() {
        return lower_while_body_block(statement.nested_statements, context, session, diagnostics, output);
    });
}

auto inferred_loop_binding_type(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    if (!statement.annotated_type.name.empty()) {
        auto source_type_name = render_source_type_name(statement.annotated_type);
        if (auto source_type = lowered_type_for_source_type_name(source_type_name, context.lowering)) {
            return source_type;
        }
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
    auto common_flow = lower_common_nonvalue_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        lower_unit_deferred_cleanup_block,
        inferred_loop_binding_type,
        "lowering does not yet support this while let type",
        "lowering does not yet support this while var type",
        [&](syntax::StatementSyntax const& nested_statement) {
            return lower_while_body_repeat(nested_statement, context, session, diagnostics, output);
        },
        [&](syntax::StatementSyntax const& nested_statement) {
            return lower_while_body_for(nested_statement, context, session, diagnostics, output);
        },
        [&](syntax::StatementSyntax const& nested_statement) {
            return lower_while_body_unsafe(nested_statement, context, session, diagnostics, output);
        }
    );
    if (common_flow.has_value()) {
        return *common_flow;
    }
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_while_body_if(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::switch_statement) {
        return lower_while_body_switch(statement, context, session, diagnostics, output);
    }
    diagnostics.error(
        statement.line,
        "lowering while body only supports local bindings, mutable-local assignments, "
        "call statements, loop control, nested if/switch statements, nested while/repeat/for statements, and unsafe blocks"
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
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(&statement);
    }

    return lower_nonvalue_statement_block(
        std::span<syntax::StatementSyntax const* const> {
            statement_pointers.data(),
            statement_pointers.size(),
        },
        "lowering does not support statements after terminating loop control",
        context,
        session,
        diagnostics,
        output,
        lower_unit_deferred_cleanup_block,
        [&](syntax::StatementSyntax const& statement, bool is_last_statement) {
            (void)is_last_statement;
            return lower_while_body_statement(statement, context, session, diagnostics, output);
        }
    );
}

auto lower_while_body_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(statement.get());
    }

    return lower_nonvalue_statement_block(
        std::span<syntax::StatementSyntax const* const> {
            statement_pointers.data(),
            statement_pointers.size(),
        },
        "lowering does not support statements after terminating loop control",
        context,
        session,
        diagnostics,
        output,
        lower_unit_deferred_cleanup_block,
        [&](syntax::StatementSyntax const& statement, bool is_last_statement) {
            (void)is_last_statement;
            return lower_while_body_statement(statement, context, session, diagnostics, output);
        }
    );
}

}  // namespace

auto lower_while_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    return lower_while_loop_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        [&]() {
            return lower_while_body_block(statement.nested_statements, context, session, diagnostics, output);
        }
    ) != StatementFlow::failed;
}

}  // namespace orison::lowering
