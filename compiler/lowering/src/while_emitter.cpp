#include "orison/lowering/while_emitter.hpp"

#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/for_loop_lowering.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/repeat_loop_lowering.hpp"
#include "orison/lowering/statement_body_lowering.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/lowering/unsafe_block_lowering.hpp"
#include "orison/lowering/while_loop_lowering.hpp"

#include <optional>
#include <string>
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
        CommonNonValueStatementPolicy {
            .infer_binding_type = inferred_loop_binding_type,
            .unsupported_let_diagnostic = "lowering does not yet support this while let type",
            .unsupported_var_diagnostic = "lowering does not yet support this while var type",
            .lower_repeat = [&](syntax::StatementSyntax const& nested_statement) {
                return lower_while_body_repeat(nested_statement, context, session, diagnostics, output);
            },
            .lower_for = [&](syntax::StatementSyntax const& nested_statement) {
                return lower_while_body_for(nested_statement, context, session, diagnostics, output);
            },
            .lower_unsafe = [&](syntax::StatementSyntax const& nested_statement) {
                return lower_while_body_unsafe(nested_statement, context, session, diagnostics, output);
            },
        }
    );
    if (common_flow.has_value()) {
        return *common_flow;
    }
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_while_body_if(statement, context, session, diagnostics, output);
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
