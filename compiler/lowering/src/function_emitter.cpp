#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_emitter.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace orison::lowering {
namespace {

using EmissionContext = ExpressionEmissionContext;
using FunctionLoweringState = ExpressionLoweringState;

auto local_value_name(std::string_view name) -> std::string {
    return "%" + std::string(name);
}
void emit_function_body(
    syntax::FunctionSyntax const& function,
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
    auto llvm_return_type = llvm_type_for(function.return_type);
    if (!llvm_return_type.has_value()) {
        diagnostics.error(function.line, "lowering does not yet support this function return type");
        return;
    }
    auto return_signedness = integer_signedness_for(function.return_type);

    auto llvm_parameter_types = llvm_parameter_types_for(function.parameters);
    if (!llvm_parameter_types.has_value()) {
        diagnostics.error(function.line, "lowering does not yet support this function parameter type");
        return;
    }
    auto parameter_signedness = parameter_signedness_for(function.parameters);

    if (*llvm_return_type == "void") {
        output << "define void @" << function.name << "(";
        for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << (*llvm_parameter_types)[index] << " " << local_value_name(function.parameters[index].name);
        }
        output << ") {\n";
        output << "entry:\n";
        output << "  ret void\n";
        output << "}\n";
        return;
    }

    output << "define " << *llvm_return_type << " @" << function.name << "(";
    for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << (*llvm_parameter_types)[index] << " " << local_value_name(function.parameters[index].name);
    }
    output << ") {\n";
    output << "entry:\n";

    auto state = FunctionLoweringState {};
    for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
        state.local_name_counts[function.parameters[index].name] = 1;
        state.immutable_bindings.emplace(function.parameters[index].name, LoweredExpression {
            .type = (*llvm_parameter_types)[index],
            .value = local_value_name(function.parameters[index].name),
            .signedness = parameter_signedness[index],
        });
    }
    auto const* expression = static_cast<syntax::ExpressionSyntax const*>(nullptr);
    auto lowered_final_statement = std::optional<LoweredExpression> {};
    for (auto index = std::size_t {0}; index < function.body_statements.size(); ++index) {
        auto const& statement = function.body_statements[index];
        auto is_last_statement = index + 1 == function.body_statements.size();
        if (!is_last_statement && statement.kind == syntax::StatementKind::let_binding) {
            if (!lower_let_statement(
                    statement,
                    *llvm_return_type,
                    return_signedness,
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
                lowered_final_statement = lower_final_control_flow_statement(
                    statement,
                    *llvm_return_type,
                    return_signedness,
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

    if (!lowered_final_statement.has_value() && expression == nullptr) {
        diagnostics.error(function.line, "lowering requires leading let bindings followed by a return or final expression");
        return;
    }

    auto lowered = std::move(lowered_final_statement);
    if (!lowered.has_value()) {
        lowered = lower_expression(*expression, *llvm_return_type, return_signedness, context, state, output);
    }
    if (!lowered.has_value()) {
        diagnostics.error(
            expression != nullptr ? expression->line : function.line,
            "lowering does not yet support this return expression"
        );
        return;
    }

    output << "  ret " << lowered->type << " " << lowered->value << "\n";
    output << "}\n";
}

}  // namespace

auto emit_function(
    syntax::FunctionSyntax const& function,
    LoweringContext const& lowering_context,
    StringConstantTable const& string_constants,
    diagnostics::DiagnosticBag& diagnostics
) -> std::string {
    auto output = std::ostringstream {};
    auto context = EmissionContext {
        .lowering = lowering_context,
        .string_constants = string_constants,
    };
    emit_function_body(function, context, diagnostics, output);
    return output.str();
}

}  // namespace orison::lowering
