#include "orison/lowering/statement_emitter.hpp"

#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"

#include <span>
#include <string>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

auto lower_value_statement_block(
    std::span<syntax::StatementSyntax const* const> statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr || statement->kind != syntax::StatementKind::let_binding ||
            !lower_let_statement(
                *statement,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                diagnostics,
                output
            )) {
            return std::nullopt;
        }
    }

    auto const* final_statement = statements.back();
    if (final_statement == nullptr) {
        return std::nullopt;
    }
    if (final_statement->kind == syntax::StatementKind::if_statement ||
        final_statement->kind == syntax::StatementKind::switch_statement) {
        return lower_final_control_flow(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }

    auto const* expression = value_expression_for(*final_statement);
    if (expression == nullptr) {
        return std::nullopt;
    }
    return lower_expression(
        *expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
}

}  // namespace

auto value_expression_for(
    syntax::StatementSyntax const& statement
) -> syntax::ExpressionSyntax const* {
    if (statement.kind == syntax::StatementKind::return_statement ||
        statement.kind == syntax::StatementKind::expression_statement) {
        return &statement.expression;
    }
    return nullptr;
}

auto lower_let_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto lowered = lower_expression(
        statement.expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            "lowering does not yet support this let initializer" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }

    auto local_name = next_llvm_local_value_name(statement.name, session.state.local_name_counts);
    output << "  " << local_name << " = add " << lowered->type << " 0, " << lowered->value << "\n";
    session.state.immutable_bindings[statement.name] = LoweredExpression {
        .type = lowered->type,
        .value = std::move(local_name),
        .signedness = lowered->signedness,
    };
    return true;
}

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow
) -> std::optional<LoweredExpression> {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(&statement);
    }
    return lower_value_statement_block(
        statement_pointers,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_final_control_flow
    );
}

auto lower_value_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow
) -> std::optional<LoweredExpression> {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(statement.get());
    }
    return lower_value_statement_block(
        statement_pointers,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_final_control_flow
    );
}

}  // namespace orison::lowering
