#include "orison/lowering/llvm_ir_emitter.hpp"

#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace orison::lowering {
namespace {

struct LoweredExpression {
    std::string type;
    std::string value;
};

auto has_generic_arguments(syntax::TypeSyntax const& type) -> bool {
    return !type.generic_arguments.empty();
}

auto llvm_type_for(syntax::TypeSyntax const& type) -> std::optional<std::string_view> {
    if (has_generic_arguments(type)) {
        return std::nullopt;
    }

    struct TypeMapping {
        std::string_view orison_type;
        std::string_view llvm_type;
    };

    constexpr auto mappings = std::array {
        TypeMapping {"Unit", "void"},
        TypeMapping {"Bool", "i1"},
        TypeMapping {"Byte", "i8"},
        TypeMapping {"UInt8", "i8"},
        TypeMapping {"Int8", "i8"},
        TypeMapping {"UInt16", "i16"},
        TypeMapping {"Int16", "i16"},
        TypeMapping {"UInt32", "i32"},
        TypeMapping {"Int32", "i32"},
        TypeMapping {"UInt64", "i64"},
        TypeMapping {"Int64", "i64"},
    };

    for (auto const& mapping : mappings) {
        if (type.name == mapping.orison_type) {
            return mapping.llvm_type;
        }
    }

    return std::nullopt;
}

auto lowered_integer_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::integer_literal) {
        return std::nullopt;
    }

    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = expression.text,
    };
}

auto lowered_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression> {
    if (auto literal = lowered_integer_literal(expression, expected_llvm_type)) {
        return literal;
    }

    if (expression.kind == syntax::ExpressionKind::cast && expression.left != nullptr) {
        syntax::TypeSyntax target_type {
            .name = expression.text,
        };
        auto cast_type = llvm_type_for(target_type);
        if (!cast_type.has_value() || *cast_type != expected_llvm_type) {
            return std::nullopt;
        }
        return lowered_integer_literal(*expression.left, expected_llvm_type);
    }

    return std::nullopt;
}

auto return_expression_for(
    syntax::FunctionSyntax const& function
) -> syntax::ExpressionSyntax const* {
    if (function.body_statements.size() != 1) {
        return nullptr;
    }

    auto const& statement = function.body_statements.front();
    if (statement.kind == syntax::StatementKind::return_statement) {
        return &statement.expression;
    }
    if (statement.kind == syntax::StatementKind::expression_statement) {
        return &statement.expression;
    }

    return nullptr;
}

void emit_function(
    syntax::FunctionSyntax const& function,
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
    if (!function.parameters.empty()) {
        diagnostics.error(function.line, "lowering does not yet support function parameters");
        return;
    }

    auto llvm_return_type = llvm_type_for(function.return_type);
    if (!llvm_return_type.has_value()) {
        diagnostics.error(function.line, "lowering does not yet support this function return type");
        return;
    }

    if (*llvm_return_type == "void") {
        output << "define void @" << function.name << "() {\n";
        output << "entry:\n";
        output << "  ret void\n";
        output << "}\n";
        return;
    }

    auto const* expression = return_expression_for(function);
    if (expression == nullptr) {
        diagnostics.error(function.line, "lowering requires a single return or final expression statement");
        return;
    }

    auto lowered = lowered_expression(*expression, *llvm_return_type);
    if (!lowered.has_value()) {
        diagnostics.error(expression->line, "lowering does not yet support this return expression");
        return;
    }

    output << "define " << lowered->type << " @" << function.name << "() {\n";
    output << "entry:\n";
    output << "  ret " << lowered->type << " " << lowered->value << "\n";
    output << "}\n";
}

}  // namespace

auto LlvmIrEmissionResult::has_errors() const -> bool {
    return diagnostics.has_errors();
}

auto LlvmIrEmissionResult::render(std::string_view path) const -> std::string {
    return diagnostics.render(path);
}

auto LlvmIrEmitter::emit(
    syntax::ModuleSyntax const& module,
    semantics::SemanticAnalysisResult const& semantic_result
) const -> LlvmIrEmissionResult {
    auto result = LlvmIrEmissionResult {};
    if (semantic_result.has_errors()) {
        result.diagnostics.error(1, "cannot lower module with semantic errors");
        return result;
    }

    if (module.functions.empty()) {
        result.diagnostics.error(1, "lowering requires at least one function");
        return result;
    }

    std::ostringstream output;
    output << "; Orison LLVM IR scaffold\n";
    if (!module.package_name.empty()) {
        output << "; package " << module.package_name << "\n";
    }
    output << "\n";

    for (auto const& function : module.functions) {
        emit_function(function, result.diagnostics, output);
        output << "\n";
    }

    if (!result.has_errors()) {
        result.ir_text = output.str();
    }
    return result;
}

}  // namespace orison::lowering
