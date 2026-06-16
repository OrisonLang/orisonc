#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/lowering/while_emitter.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace orison::lowering {
namespace {

using EmissionContext = LoweringEmissionContext;

auto is_empty_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.text.empty() && expression.arguments.empty() && expression.nested_statements.empty() &&
           expression.left == nullptr && expression.right == nullptr && expression.alternate == nullptr;
}

auto infer_unit_binding_type(
    syntax::StatementSyntax const& statement,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    if (!statement.annotated_type.name.empty()) {
        auto annotated_type = llvm_type_for(statement.annotated_type);
        if (!annotated_type.has_value() || *annotated_type == "void") {
            return std::nullopt;
        }
        return LoweredType {
            .type = std::string(*annotated_type),
            .signedness = integer_signedness_for(statement.annotated_type),
        };
    }

    auto inferred_type = infer_expression_type(statement.expression, context, state);
    if (!inferred_type.has_value() || inferred_type->type.empty() || inferred_type->type == "void") {
        return std::nullopt;
    }
    return inferred_type;
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
    if (function.is_unsafe) {
        diagnostics.error(function.line, "lowering does not yet support unsafe functions");
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
        state.source_type_names.emplace(
            function.parameters[index].name,
            render_source_type_name(function.parameters[index].type)
        );
    }

    if (signature.return_type == "void") {
        for (auto index = std::size_t {0}; index < function.body_statements.size(); ++index) {
            auto const& statement = function.body_statements[index];
            auto is_last_statement = index + 1 == function.body_statements.size();
            if (statement.kind == syntax::StatementKind::return_statement) {
                if (!is_last_statement) {
                    diagnostics.error(
                        statement.line,
                        "lowering does not yet support return statements before the end of Unit functions"
                    );
                    return;
                }
                if (!is_empty_expression(statement.expression)) {
                    diagnostics.error(
                        statement.line,
                        "lowering does not yet support return expressions in Unit functions"
                    );
                    return;
                }
                output << "  ret void\n";
                output << "}\n";
                return;
            }
            if (statement.kind == syntax::StatementKind::let_binding) {
                auto type = infer_unit_binding_type(statement, context, session.state);
                if (!type.has_value()) {
                    diagnostics.error(statement.line, "lowering does not yet support this Unit let binding");
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
            if (statement.kind == syntax::StatementKind::var_binding) {
                auto type = infer_unit_binding_type(statement, context, session.state);
                if (!type.has_value()) {
                    diagnostics.error(statement.line, "lowering does not yet support this Unit var binding");
                    return;
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
                    return;
                }
                continue;
            }
            if (statement.kind == syntax::StatementKind::assignment_statement) {
                if (!lower_assignment_statement(statement, context, session, diagnostics, output)) {
                    return;
                }
                continue;
            }
            if (statement.kind == syntax::StatementKind::while_statement) {
                if (!lower_while_statement(statement, context, session, diagnostics, output)) {
                    return;
                }
                continue;
            }
            if (statement.kind == syntax::StatementKind::expression_statement &&
                statement.expression.kind == syntax::ExpressionKind::call) {
                if (!lower_call_statement(statement, context, session, diagnostics, output)) {
                    return;
                }
                continue;
            }
            diagnostics.error(statement.line, "lowering does not yet support this statement");
            return;
        }

        output << "  ret void\n";
        output << "}\n";
        return;
    }

    auto const* expression = static_cast<syntax::ExpressionSyntax const*>(nullptr);
    auto lowered_final_statement = std::optional<LoweredExpression> {};
    auto attempted_final_control_flow = false;
    auto final_statement_line = function.line;
    for (auto index = std::size_t {0}; index < function.body_statements.size(); ++index) {
        auto const& statement = function.body_statements[index];
        auto is_last_statement = index + 1 == function.body_statements.size();
        if (!is_last_statement && statement.kind == syntax::StatementKind::let_binding) {
            if (!lower_let_statement(
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
        if (!is_last_statement && statement.kind == syntax::StatementKind::while_statement) {
            if (!lower_while_statement(statement, context, session, diagnostics, output)) {
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

    output << "  ret " << lowered->type << " " << lowered->value << "\n";
    output << "}\n";
}

}  // namespace

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
