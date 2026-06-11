#include "orison/lowering/llvm_ir_emitter.hpp"

#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

enum class IntegerSignedness {
    not_integer,
    signed_integer,
    unsigned_integer,
};

struct LoweredType {
    std::string type;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

struct LoweredExpression {
    std::string type;
    std::string value;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

struct FunctionSignature {
    std::string return_type;
    IntegerSignedness return_signedness = IntegerSignedness::not_integer;
    std::vector<std::string> parameter_types;
    std::vector<IntegerSignedness> parameter_signedness;
};

struct LoweringContext {
    std::unordered_map<std::string, FunctionSignature> functions;
};

struct FunctionLoweringState {
    std::unordered_map<std::string, LoweredExpression> immutable_bindings;
    std::unordered_map<std::string, std::size_t> local_name_counts;
    std::size_t next_temporary_index = 0;
    std::size_t next_block_index = 0;
    std::string current_block = "entry";
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

auto integer_signedness_for(syntax::TypeSyntax const& type) -> IntegerSignedness {
    if (has_generic_arguments(type)) {
        return IntegerSignedness::not_integer;
    }
    if (type.name == "Byte" || type.name == "UInt8" || type.name == "UInt16" || type.name == "UInt32" ||
        type.name == "UInt64") {
        return IntegerSignedness::unsigned_integer;
    }
    if (type.name == "Int8" || type.name == "Int16" || type.name == "Int32" || type.name == "Int64") {
        return IntegerSignedness::signed_integer;
    }
    return IntegerSignedness::not_integer;
}

auto lowered_integer_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::integer_literal) {
        return std::nullopt;
    }

    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = expression.text,
        .signedness = expected_signedness,
    };
}

auto lowered_boolean_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::boolean_literal || expected_llvm_type != "i1") {
        return std::nullopt;
    }

    return LoweredExpression {
        .type = "i1",
        .value = expression.text == "true" ? "1" : "0",
        .signedness = IntegerSignedness::not_integer,
    };
}

auto llvm_binary_instruction_for(
    std::string_view operator_text,
    IntegerSignedness signedness
) -> std::optional<std::string_view> {
    if (operator_text == "+") {
        return "add";
    }
    if (operator_text == "-") {
        return "sub";
    }
    if (operator_text == "*") {
        return "mul";
    }
    if (operator_text == "/") {
        return signedness == IntegerSignedness::signed_integer ? "sdiv" : "udiv";
    }
    return std::nullopt;
}

auto llvm_integer_comparison_predicate_for(
    std::string_view operator_text,
    IntegerSignedness signedness
) -> std::optional<std::string_view> {
    if (operator_text == "==") {
        return "eq";
    }
    if (operator_text == "!=") {
        return "ne";
    }
    if (operator_text == "<") {
        return signedness == IntegerSignedness::signed_integer ? "slt" : "ult";
    }
    if (operator_text == "<=") {
        return signedness == IntegerSignedness::signed_integer ? "sle" : "ule";
    }
    if (operator_text == ">") {
        return signedness == IntegerSignedness::signed_integer ? "sgt" : "ugt";
    }
    if (operator_text == ">=") {
        return signedness == IntegerSignedness::signed_integer ? "sge" : "uge";
    }
    return std::nullopt;
}

auto is_integer_llvm_type(std::string_view type) -> bool {
    return type == "i8" || type == "i16" || type == "i32" || type == "i64";
}

auto inferred_expression_type(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto binding = state.immutable_bindings.find(expression.text);
        if (binding == state.immutable_bindings.end()) {
            return std::nullopt;
        }
        return LoweredType {
            .type = binding->second.type,
            .signedness = binding->second.signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::cast) {
        syntax::TypeSyntax target_type {
            .name = expression.text,
        };
        auto cast_type = llvm_type_for(target_type);
        if (!cast_type.has_value()) {
            return std::nullopt;
        }
        return LoweredType {
            .type = std::string(*cast_type),
            .signedness = integer_signedness_for(target_type),
        };
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        auto function = context.functions.find(expression.left->text);
        if (function == context.functions.end()) {
            return std::nullopt;
        }
        return LoweredType {
            .type = function->second.return_type,
            .signedness = function->second.return_signedness,
        };
    }

    return std::nullopt;
}

auto lowered_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringContext const& context,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (auto literal = lowered_integer_literal(expression, expected_llvm_type, expected_signedness)) {
        return literal;
    }
    if (auto literal = lowered_boolean_literal(expression, expected_llvm_type)) {
        return literal;
    }

    if (expression.kind == syntax::ExpressionKind::name) {
        auto binding = state.immutable_bindings.find(expression.text);
        if (binding == state.immutable_bindings.end()) {
            return std::nullopt;
        }
        if (binding->second.type != expected_llvm_type) {
            return std::nullopt;
        }
        if (binding->second.signedness != expected_signedness) {
            return std::nullopt;
        }
        return binding->second;
    }

    if (expression.kind == syntax::ExpressionKind::cast && expression.left != nullptr) {
        syntax::TypeSyntax target_type {
            .name = expression.text,
        };
        auto cast_type = llvm_type_for(target_type);
        if (!cast_type.has_value() || *cast_type != expected_llvm_type) {
            return std::nullopt;
        }
        return lowered_integer_literal(*expression.left, expected_llvm_type, integer_signedness_for(target_type));
    }

    if (expression.kind == syntax::ExpressionKind::unary && expression.text == "not" &&
        expression.left != nullptr && expected_llvm_type == "i1") {
        auto operand = lowered_expression(
            *expression.left,
            "i1",
            IntegerSignedness::not_integer,
            context,
            state,
            output
        );
        if (!operand.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
        output << "  " << temporary_name << " = xor i1 " << operand->value << ", true\n";
        return LoweredExpression {
            .type = "i1",
            .value = std::move(temporary_name),
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (expression.kind == syntax::ExpressionKind::ternary && expression.left != nullptr &&
        expression.right != nullptr && expression.alternate != nullptr) {
        auto condition = lowered_expression(
            *expression.left,
            "i1",
            IntegerSignedness::not_integer,
            context,
            state,
            output
        );
        if (!condition.has_value()) {
            return std::nullopt;
        }

        auto block_index = state.next_block_index++;
        auto then_block = "ternary.then." + std::to_string(block_index);
        auto else_block = "ternary.else." + std::to_string(block_index);
        auto merge_block = "ternary.merge." + std::to_string(block_index);
        output << "  br i1 " << condition->value << ", label %" << then_block << ", label %" << else_block << "\n";

        output << then_block << ":\n";
        state.current_block = then_block;
        auto then_value = lowered_expression(
            *expression.right,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            output
        );
        if (!then_value.has_value()) {
            return std::nullopt;
        }
        auto then_exit_block = state.current_block;
        output << "  br label %" << merge_block << "\n";

        output << else_block << ":\n";
        state.current_block = else_block;
        auto else_value = lowered_expression(
            *expression.alternate,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            output
        );
        if (!else_value.has_value() || then_value->type != else_value->type ||
            then_value->signedness != else_value->signedness) {
            return std::nullopt;
        }
        auto else_exit_block = state.current_block;
        output << "  br label %" << merge_block << "\n";

        output << merge_block << ":\n";
        state.current_block = merge_block;
        auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
        output << "  " << temporary_name << " = phi " << then_value->type << " [" << then_value->value;
        output << ", %" << then_exit_block << "], [" << else_value->value << ", %" << else_exit_block << "]\n";
        return LoweredExpression {
            .type = then_value->type,
            .value = std::move(temporary_name),
            .signedness = then_value->signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::binary && expression.left != nullptr &&
        expression.right != nullptr) {
        if (expected_llvm_type == "i1") {
            if (expression.text == "and" || expression.text == "or") {
                auto left = lowered_expression(
                    *expression.left,
                    "i1",
                    IntegerSignedness::not_integer,
                    context,
                    state,
                    output
                );
                auto right = lowered_expression(
                    *expression.right,
                    "i1",
                    IntegerSignedness::not_integer,
                    context,
                    state,
                    output
                );
                if (!left.has_value() || !right.has_value()) {
                    return std::nullopt;
                }

                auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
                output << "  " << temporary_name << " = " << expression.text << " i1 ";
                output << left->value << ", " << right->value << "\n";
                return LoweredExpression {
                    .type = "i1",
                    .value = std::move(temporary_name),
                    .signedness = IntegerSignedness::not_integer,
                };
            }

            auto operand_type = inferred_expression_type(*expression.left, context, state);
            if (!operand_type.has_value()) {
                operand_type = inferred_expression_type(*expression.right, context, state);
            }
            if (!operand_type.has_value() || !is_integer_llvm_type(operand_type->type) ||
                operand_type->signedness == IntegerSignedness::not_integer) {
                return std::nullopt;
            }

            auto predicate = llvm_integer_comparison_predicate_for(expression.text, operand_type->signedness);
            if (predicate.has_value()) {
                auto left = lowered_expression(
                    *expression.left,
                    operand_type->type,
                    operand_type->signedness,
                    context,
                    state,
                    output
                );
                auto right = lowered_expression(
                    *expression.right,
                    operand_type->type,
                    operand_type->signedness,
                    context,
                    state,
                    output
                );
                if (!left.has_value() || !right.has_value() || left->type != right->type ||
                    left->signedness != right->signedness) {
                    return std::nullopt;
                }

                auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
                output << "  " << temporary_name << " = icmp " << *predicate << " " << left->type << " ";
                output << left->value << ", " << right->value << "\n";
                return LoweredExpression {
                    .type = "i1",
                    .value = std::move(temporary_name),
                    .signedness = IntegerSignedness::not_integer,
                };
            }
        }

        auto instruction = llvm_binary_instruction_for(expression.text, expected_signedness);
        if (!instruction.has_value()) {
            return std::nullopt;
        }
        auto left = lowered_expression(*expression.left, expected_llvm_type, expected_signedness, context, state, output);
        auto right = lowered_expression(*expression.right, expected_llvm_type, expected_signedness, context, state, output);
        if (!left.has_value() || !right.has_value() || left->type != right->type ||
            left->signedness != right->signedness) {
            return std::nullopt;
        }

        auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
        output << "  " << temporary_name << " = " << *instruction << " " << left->type << " " << left->value << ", ";
        output << right->value << "\n";
        return LoweredExpression {
            .type = left->type,
            .value = std::move(temporary_name),
            .signedness = left->signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        auto function = context.functions.find(expression.left->text);
        if (function == context.functions.end() || function->second.return_type != expected_llvm_type) {
            return std::nullopt;
        }
        if (function->second.parameter_types.size() != expression.arguments.size()) {
            return std::nullopt;
        }

        auto arguments = std::vector<LoweredExpression> {};
        arguments.reserve(expression.arguments.size());
        for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
            auto argument = lowered_expression(
                expression.arguments[index],
                function->second.parameter_types[index],
                function->second.parameter_signedness[index],
                context,
                state,
                output
            );
            if (!argument.has_value()) {
                return std::nullopt;
            }
            arguments.push_back(std::move(*argument));
        }

        auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
        output << "  " << temporary_name << " = call " << function->second.return_type;
        output << " @" << expression.left->text << "(";
        for (auto index = std::size_t {0}; index < arguments.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << arguments[index].type << " " << arguments[index].value;
        }
        output << ")\n";
        return LoweredExpression {
            .type = function->second.return_type,
            .value = std::move(temporary_name),
            .signedness = function->second.return_signedness,
        };
    }

    return std::nullopt;
}

auto return_expression_for(syntax::StatementSyntax const& statement) -> syntax::ExpressionSyntax const* {
    if (statement.kind == syntax::StatementKind::return_statement) {
        return &statement.expression;
    }
    if (statement.kind == syntax::StatementKind::expression_statement) {
        return &statement.expression;
    }

    return nullptr;
}

auto local_value_name(std::string_view name) -> std::string {
    return "%" + std::string(name);
}

auto next_local_value_name(std::string_view name, FunctionLoweringState& state) -> std::string {
    auto& count = state.local_name_counts[std::string(name)];
    auto value_name = local_value_name(name);
    if (count > 0) {
        value_name += "." + std::to_string(count);
    }
    ++count;
    return value_name;
}

auto llvm_parameter_types_for(
    std::vector<syntax::ParameterSyntax> const& parameters
) -> std::optional<std::vector<std::string>> {
    auto parameter_types = std::vector<std::string> {};
    parameter_types.reserve(parameters.size());
    for (auto const& parameter : parameters) {
        auto parameter_type = llvm_type_for(parameter.type);
        if (!parameter_type.has_value() || *parameter_type == "void") {
            return std::nullopt;
        }
        parameter_types.emplace_back(*parameter_type);
    }
    return parameter_types;
}

auto parameter_signedness_for(
    std::vector<syntax::ParameterSyntax> const& parameters
) -> std::vector<IntegerSignedness> {
    auto signedness = std::vector<IntegerSignedness> {};
    signedness.reserve(parameters.size());
    for (auto const& parameter : parameters) {
        signedness.push_back(integer_signedness_for(parameter.type));
    }
    return signedness;
}

auto lower_let_binding(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto lowered = lowered_expression(statement.expression, expected_llvm_type, expected_signedness, context, state, output);
    if (!lowered.has_value()) {
        diagnostics.error(statement.line, "lowering does not yet support this let initializer");
        return false;
    }

    auto local_name = next_local_value_name(statement.name, state);
    output << "  " << local_name << " = add " << lowered->type << " 0, " << lowered->value << "\n";
    state.immutable_bindings[statement.name] = LoweredExpression {
        .type = lowered->type,
        .value = std::move(local_name),
        .signedness = lowered->signedness,
    };
    return true;
}

auto lower_final_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const& statement = statements[index];
        if (statement.kind != syntax::StatementKind::let_binding ||
            !lower_let_binding(
                statement,
                expected_llvm_type,
                expected_signedness,
                context,
                state,
                diagnostics,
                output
            )) {
            return std::nullopt;
        }
    }

    auto const& final_statement = statements.back();
    if (final_statement.kind == syntax::StatementKind::if_statement) {
        return lower_final_if_statement(
            final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
    }

    auto const* expression = return_expression_for(final_statement);
    if (expression == nullptr) {
        return std::nullopt;
    }
    return lowered_expression(
        *expression,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        output
    );
}

auto lower_final_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statement.kind != syntax::StatementKind::if_statement || statement.nested_statements.empty() ||
        statement.alternate_statements.empty()) {
        return std::nullopt;
    }

    auto condition = lowered_expression(
        statement.expression,
        "i1",
        IntegerSignedness::not_integer,
        context,
        state,
        output
    );
    if (!condition.has_value()) {
        return std::nullopt;
    }

    auto block_index = state.next_block_index++;
    auto then_block = "if.then." + std::to_string(block_index);
    auto else_block = "if.else." + std::to_string(block_index);
    auto merge_block = "if.merge." + std::to_string(block_index);
    output << "  br i1 " << condition->value << ", label %" << then_block << ", label %" << else_block << "\n";

    output << then_block << ":\n";
    state.current_block = then_block;
    auto outer_bindings = state.immutable_bindings;
    auto then_value = lower_value_statement_block(
        statement.nested_statements,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        diagnostics,
        output
    );
    if (!then_value.has_value()) {
        return std::nullopt;
    }
    auto then_exit_block = state.current_block;
    output << "  br label %" << merge_block << "\n";

    output << else_block << ":\n";
    state.current_block = else_block;
    state.immutable_bindings = outer_bindings;
    auto else_value = lower_value_statement_block(
        statement.alternate_statements,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        diagnostics,
        output
    );
    if (!else_value.has_value() || then_value->type != else_value->type ||
        then_value->signedness != else_value->signedness) {
        return std::nullopt;
    }
    auto else_exit_block = state.current_block;
    output << "  br label %" << merge_block << "\n";

    output << merge_block << ":\n";
    state.current_block = merge_block;
    state.immutable_bindings = std::move(outer_bindings);
    auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
    output << "  " << temporary_name << " = phi " << then_value->type << " [" << then_value->value;
    output << ", %" << then_exit_block << "], [" << else_value->value << ", %" << else_exit_block << "]\n";
    return LoweredExpression {
        .type = then_value->type,
        .value = std::move(temporary_name),
        .signedness = then_value->signedness,
    };
}

void emit_function(
    syntax::FunctionSyntax const& function,
    LoweringContext const& context,
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
            if (!lower_let_binding(
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
            if (statement.kind == syntax::StatementKind::if_statement) {
                lowered_final_statement = lower_final_if_statement(
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
            expression = return_expression_for(statement);
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
        lowered = lowered_expression(*expression, *llvm_return_type, return_signedness, context, state, output);
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

auto collect_lowering_context(syntax::ModuleSyntax const& module) -> LoweringContext {
    auto context = LoweringContext {};
    for (auto const& function : module.functions) {
        auto return_type = llvm_type_for(function.return_type);
        if (!return_type.has_value()) {
            continue;
        }
        auto parameter_types = llvm_parameter_types_for(function.parameters);
        if (!parameter_types.has_value()) {
            continue;
        }
        context.functions.emplace(function.name, FunctionSignature {
            .return_type = std::string(*return_type),
            .return_signedness = integer_signedness_for(function.return_type),
            .parameter_types = std::move(*parameter_types),
            .parameter_signedness = parameter_signedness_for(function.parameters),
        });
    }
    return context;
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

    auto context = collect_lowering_context(module);
    for (auto const& function : module.functions) {
        emit_function(function, context, result.diagnostics, output);
        output << "\n";
    }

    if (!result.has_errors()) {
        result.ir_text = output.str();
    }
    return result;
}

}  // namespace orison::lowering
