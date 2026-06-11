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
#include <vector>

namespace orison::lowering {
namespace {

struct LoweredType {
    std::string type;
    IntegerSignedness signedness = IntegerSignedness::not_integer;
};

using EmissionContext = ExpressionEmissionContext;
using FunctionLoweringState = ExpressionLoweringState;

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

auto lowered_float_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::float_literal ||
        (expected_llvm_type != "float" && expected_llvm_type != "double")) {
        return std::nullopt;
    }

    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = expression.text,
        .signedness = IntegerSignedness::not_integer,
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
    EmissionContext const& context,
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
        auto function = context.lowering.functions.find(expression.left->text);
        if (function == context.lowering.functions.end()) {
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
    EmissionContext const& context,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (auto literal = lowered_integer_literal(expression, expected_llvm_type, expected_signedness)) {
        return literal;
    }
    if (auto literal = lowered_float_literal(expression, expected_llvm_type)) {
        return literal;
    }
    if (auto literal = lowered_boolean_literal(expression, expected_llvm_type)) {
        return literal;
    }
    if (expression.kind == syntax::ExpressionKind::string_literal && expected_llvm_type == "ptr") {
        auto const* constant = context.string_constants.find(expression.text);
        if (constant == nullptr) {
            return std::nullopt;
        }
        return LoweredExpression {
            .type = "ptr",
            .value = "@" + constant->name,
            .signedness = IntegerSignedness::not_integer,
        };
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
        if (auto integer = lowered_integer_literal(
                *expression.left,
                expected_llvm_type,
                integer_signedness_for(target_type)
            )) {
            return integer;
        }
        return lowered_float_literal(*expression.left, expected_llvm_type);
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
        auto function = context.lowering.functions.find(expression.left->text);
        if (function == context.lowering.functions.end() ||
            function->second.return_type != expected_llvm_type) {
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
            auto promotion = index >= function->second.fixed_abi_parameter_count
                ? c_abi_promotion_for(argument->type)
                : CAbiPromotion::none;
            if (function->second.adapter == CAbiAdapterKind::variadic &&
                promotion == CAbiPromotion::integer_to_i32) {
                auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
                auto instruction =
                    argument->signedness == IntegerSignedness::signed_integer ? "sext" : "zext";
                output << "  " << temporary_name << " = " << instruction << " " << argument->type << " ";
                output << argument->value << " to i32\n";
                argument = LoweredExpression {
                    .type = "i32",
                    .value = std::move(temporary_name),
                    .signedness = argument->signedness,
                };
            }
            if (function->second.adapter == CAbiAdapterKind::variadic &&
                promotion == CAbiPromotion::float_to_double) {
                auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
                output << "  " << temporary_name << " = fpext float " << argument->value << " to double\n";
                argument = LoweredExpression {
                    .type = "double",
                    .value = std::move(temporary_name),
                    .signedness = IntegerSignedness::not_integer,
                };
            }
            arguments.push_back(std::move(*argument));
        }

        auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
        output << "  " << temporary_name << " = call " << function->second.return_type;
        if (function->second.adapter == CAbiAdapterKind::variadic) {
            output << " (";
            for (auto index = std::size_t {0}; index < function->second.fixed_abi_parameter_count; ++index) {
                if (index > 0) {
                    output << ", ";
                }
                output << function->second.parameter_types[index];
            }
            if (function->second.fixed_abi_parameter_count > 0) {
                output << ", ";
            }
            output << "...)";
        }
        output << " @" << function->second.symbol_name << "(";
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

auto lower_let_binding(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
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
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto lower_final_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
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
    if (final_statement.kind == syntax::StatementKind::switch_statement) {
        return lower_final_switch_statement(
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
    EmissionContext const& context,
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

auto lower_switch_case_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const* statement = statements[index].get();
        if (statement == nullptr || statement->kind != syntax::StatementKind::let_binding ||
            !lower_let_binding(
                *statement,
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

    auto const* final_statement = statements.back().get();
    if (final_statement == nullptr) {
        return std::nullopt;
    }
    if (final_statement->kind == syntax::StatementKind::if_statement) {
        return lower_final_if_statement(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
    }
    if (final_statement->kind == syntax::StatementKind::switch_statement) {
        return lower_final_switch_statement(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
    }

    auto const* expression = return_expression_for(*final_statement);
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

auto lowered_switch_pattern(
    syntax::ExpressionSyntax const& pattern,
    LoweredType const& subject_type
) -> std::optional<LoweredExpression> {
    if (auto literal = lowered_integer_literal(pattern, subject_type.type, subject_type.signedness)) {
        return literal;
    }
    if (auto literal = lowered_boolean_literal(pattern, subject_type.type)) {
        return literal;
    }
    if (pattern.kind != syntax::ExpressionKind::cast || pattern.left == nullptr) {
        return std::nullopt;
    }

    syntax::TypeSyntax target_type {
        .name = pattern.text,
    };
    auto cast_type = llvm_type_for(target_type);
    if (!cast_type.has_value() || *cast_type != subject_type.type) {
        return std::nullopt;
    }
    return lowered_integer_literal(*pattern.left, subject_type.type, subject_type.signedness);
}

auto lower_final_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringState& state,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statement.kind != syntax::StatementKind::switch_statement || statement.switch_cases.empty()) {
        return std::nullopt;
    }

    auto subject_type = inferred_expression_type(statement.expression, context, state);
    if (!subject_type.has_value() ||
        (subject_type->type != "i1" && !is_integer_llvm_type(subject_type->type))) {
        return std::nullopt;
    }
    auto subject = lowered_expression(
        statement.expression,
        subject_type->type,
        subject_type->signedness,
        context,
        state,
        output
    );
    if (!subject.has_value()) {
        return std::nullopt;
    }

    struct LoweredSwitchCase {
        syntax::SwitchCaseSyntax const* syntax = nullptr;
        std::string block;
        std::optional<LoweredExpression> pattern;
    };

    auto block_index = state.next_block_index++;
    auto merge_block = "switch.merge." + std::to_string(block_index);
    auto fallback_block = "switch.unreachable." + std::to_string(block_index);
    auto lowered_cases = std::vector<LoweredSwitchCase> {};
    lowered_cases.reserve(statement.switch_cases.size());
    auto default_case_index = std::optional<std::size_t> {};
    auto value_case_index = std::size_t {0};

    for (auto const& switch_case : statement.switch_cases) {
        auto lowered_case = LoweredSwitchCase {
            .syntax = &switch_case,
        };
        if (switch_case.is_default) {
            if (default_case_index.has_value()) {
                return std::nullopt;
            }
            lowered_case.block = "switch.default." + std::to_string(block_index);
            default_case_index = lowered_cases.size();
        } else {
            lowered_case.block =
                "switch.case." + std::to_string(block_index) + "." + std::to_string(value_case_index++);
            lowered_case.pattern = lowered_switch_pattern(switch_case.pattern, *subject_type);
            if (!lowered_case.pattern.has_value()) {
                return std::nullopt;
            }
        }
        lowered_cases.push_back(std::move(lowered_case));
    }

    auto const& default_block =
        default_case_index.has_value() ? lowered_cases[*default_case_index].block : fallback_block;
    output << "  switch " << subject->type << " " << subject->value << ", label %" << default_block << " [\n";
    for (auto const& lowered_case : lowered_cases) {
        if (lowered_case.pattern.has_value()) {
            output << "    " << subject->type << " " << lowered_case.pattern->value;
            output << ", label %" << lowered_case.block << "\n";
        }
    }
    output << "  ]\n";

    auto outer_bindings = state.immutable_bindings;
    auto incoming_values = std::vector<std::pair<LoweredExpression, std::string>> {};
    incoming_values.reserve(lowered_cases.size());
    for (auto const& lowered_case : lowered_cases) {
        output << lowered_case.block << ":\n";
        state.current_block = lowered_case.block;
        state.immutable_bindings = outer_bindings;
        auto case_value = lower_switch_case_statement_block(
            lowered_case.syntax->statements,
            expected_llvm_type,
            expected_signedness,
            context,
            state,
            diagnostics,
            output
        );
        if (!case_value.has_value()) {
            return std::nullopt;
        }
        auto case_exit_block = state.current_block;
        output << "  br label %" << merge_block << "\n";
        incoming_values.emplace_back(std::move(*case_value), std::move(case_exit_block));
    }

    if (!default_case_index.has_value()) {
        output << fallback_block << ":\n";
        output << "  unreachable\n";
    }

    if (incoming_values.empty()) {
        return std::nullopt;
    }
    auto const& result_type = incoming_values.front().first;
    for (auto const& [incoming_value, incoming_block] : incoming_values) {
        static_cast<void>(incoming_block);
        if (incoming_value.type != result_type.type || incoming_value.signedness != result_type.signedness) {
            return std::nullopt;
        }
    }

    output << merge_block << ":\n";
    state.current_block = merge_block;
    state.immutable_bindings = std::move(outer_bindings);
    auto temporary_name = "%tmp" + std::to_string(state.next_temporary_index++);
    output << "  " << temporary_name << " = phi " << result_type.type << " ";
    for (auto index = std::size_t {0}; index < incoming_values.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << "[" << incoming_values[index].first.value << ", %" << incoming_values[index].second << "]";
    }
    output << "\n";
    return LoweredExpression {
        .type = result_type.type,
        .value = std::move(temporary_name),
        .signedness = result_type.signedness,
    };
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
            if (statement.kind == syntax::StatementKind::switch_statement) {
                lowered_final_statement = lower_final_switch_statement(
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

}  // namespace

auto lower_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    ExpressionEmissionContext const& context,
    ExpressionLoweringState& state,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    return lowered_expression(
        expression,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        output
    );
}

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
