#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/string_constants.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace orison::lowering {
namespace {

using EmissionContext = ExpressionEmissionContext;

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

    if (signature.return_type == "void") {
        output << "define void @" << function.name << "(";
        for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << signature.parameter_types[index] << " "
                   << llvm_local_value_name(function.parameters[index].name);
        }
        output << ") {\n";
        output << "entry:\n";
        output << "  ret void\n";
        output << "}\n";
        return;
    }

    output << "define " << signature.return_type << " @" << function.name << "(";
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
    for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
        state.local_name_counts[function.parameters[index].name] = 1;
        state.immutable_bindings.emplace(function.parameters[index].name, LoweredExpression {
            .type = signature.parameter_types[index],
            .value = llvm_local_value_name(function.parameters[index].name),
            .signedness = signature.parameter_signedness[index],
        });
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
                    state,
                    diagnostics,
                    output
                )) {
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
                    state,
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
        auto detail = render_control_flow_lowering_failure(state.control_flow_failure);
        diagnostics.error(
            final_statement_line,
            "lowering does not yet support this final control-flow statement" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return;
    }

    if (!lowered_final_statement.has_value() && expression == nullptr) {
        diagnostics.error(function.line, "lowering requires leading let bindings followed by a return or final expression");
        return;
    }

    auto lowered = std::move(lowered_final_statement);
    if (!lowered.has_value()) {
        lowered = lower_expression(
            *expression,
            signature.return_type,
            signature.return_signedness,
            context,
            state,
            output
        );
    }
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(state.expression_failure);
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
