#include "orison/lowering/while_emitter.hpp"

#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/for_loop_lowering.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/nonvalue_if_lowering.hpp"
#include "orison/lowering/nonvalue_switch_lowering.hpp"
#include "orison/lowering/repeat_loop_lowering.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/statement_body_lowering.hpp"
#include "orison/lowering/statement_pointer_adapter.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/lowering/unsafe_block_lowering.hpp"
#include "orison/lowering/unit_deferred_cleanup.hpp"
#include "orison/lowering/while_loop_lowering.hpp"

#include <memory>
#include <optional>
#include <span>
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

    return lower_nonvalue_if_statement(
        statement,
        context,
        session,
        diagnostics,
        output,
        "lowering does not yet support this while-body if condition",
        false,
        [&]() {
            return lower_while_body_block(statement.nested_statements, context, session, diagnostics, output);
        },
        [&]() {
            return lower_while_body_block(
                statement.alternate_statements,
                context,
                session,
                diagnostics,
                output
            );
        }
    );
}

auto lower_while_body_switch(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> StatementFlow {
    auto subject_type = infer_expression_type(statement.expression, context, session.state);
    if (!subject_type.has_value()) {
        diagnostics.error(statement.line, "lowering does not yet support this while-body switch subject");
        return StatementFlow::failed;
    }

    return lower_nonvalue_switch_statement(
        statement,
        *subject_type,
        context,
        session,
        diagnostics,
        output,
        "lowering does not yet support this while-body switch subject",
        "lowering does not yet support this while-body switch statement",
        [&](LoweredSwitchCasePlan const& planned_case) {
            return lower_while_body_block(
                planned_case.syntax->statements,
                context,
                session,
                diagnostics,
                output
            );
        }
    );
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
    auto statement_pointers = statement_pointers_for(statements);

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
    auto statement_pointers = statement_pointers_for(statements);

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
