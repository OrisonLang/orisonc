#include "orison/lowering/statement_emitter.hpp"

#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"

#include <string>
#include <utility>

namespace orison::lowering {

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

}  // namespace orison::lowering
