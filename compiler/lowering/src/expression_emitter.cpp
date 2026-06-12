#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/llvm_names.hpp"
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

using EmissionContext = ExpressionEmissionContext;

void record_failure(
    FunctionLoweringState& state,
    ExpressionLoweringFailureReason reason,
    std::string detail
) {
    if (state.expression_failure.reason == ExpressionLoweringFailureReason::none) {
        state.expression_failure = ExpressionLoweringFailure {
            .reason = reason,
            .detail = std::move(detail),
        };
    }
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

auto is_integer_llvm_type_impl(std::string_view type) -> bool {
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
            record_failure(
                state,
                ExpressionLoweringFailureReason::missing_string_constant,
                expression.text
            );
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
            record_failure(state, ExpressionLoweringFailureReason::unknown_name, expression.text);
            return std::nullopt;
        }
        if (binding->second.type != expected_llvm_type) {
            record_failure(
                state,
                ExpressionLoweringFailureReason::type_mismatch,
                expression.text + " has LLVM type " + binding->second.type +
                    ", expected " + std::string(expected_llvm_type)
            );
            return std::nullopt;
        }
        if (binding->second.signedness != expected_signedness) {
            record_failure(
                state,
                ExpressionLoweringFailureReason::signedness_mismatch,
                expression.text
            );
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
            record_failure(
                state,
                ExpressionLoweringFailureReason::unsupported_cast,
                expression.text
            );
            return std::nullopt;
        }
        if (auto integer = lowered_integer_literal(
                *expression.left,
                expected_llvm_type,
                integer_signedness_for(target_type)
            )) {
            return integer;
        }
        auto lowered_float = lowered_float_literal(*expression.left, expected_llvm_type);
        if (!lowered_float.has_value()) {
            record_failure(
                state,
                ExpressionLoweringFailureReason::unsupported_cast,
                expression.text
            );
        }
        return lowered_float;
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

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
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

        auto block_index = next_llvm_block_index(state.next_block_index);
        auto then_block = llvm_block_name("ternary.then", block_index);
        auto else_block = llvm_block_name("ternary.else", block_index);
        auto merge_block = llvm_block_name("ternary.merge", block_index);
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
        if (!else_value.has_value()) {
            return std::nullopt;
        }
        if (then_value->type != else_value->type ||
            then_value->signedness != else_value->signedness) {
            record_failure(
                state,
                ExpressionLoweringFailureReason::branch_type_mismatch,
                "ternary branches"
            );
            return std::nullopt;
        }
        auto else_exit_block = state.current_block;
        output << "  br label %" << merge_block << "\n";

        output << merge_block << ":\n";
        state.current_block = merge_block;
        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
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

                auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
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
            if (!operand_type.has_value() || !is_integer_llvm_type_impl(operand_type->type) ||
                operand_type->signedness == IntegerSignedness::not_integer) {
                record_failure(
                    state,
                    ExpressionLoweringFailureReason::cannot_infer_operand_type,
                    expression.text
                );
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

                auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
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
            record_failure(
                state,
                ExpressionLoweringFailureReason::unsupported_operator,
                expression.text
            );
            return std::nullopt;
        }
        auto left = lowered_expression(*expression.left, expected_llvm_type, expected_signedness, context, state, output);
        auto right = lowered_expression(*expression.right, expected_llvm_type, expected_signedness, context, state, output);
        if (!left.has_value() || !right.has_value() || left->type != right->type ||
            left->signedness != right->signedness) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
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
        if (function == context.lowering.functions.end()) {
            record_failure(
                state,
                ExpressionLoweringFailureReason::unknown_function,
                expression.left->text
            );
            return std::nullopt;
        }
        if (function->second.return_type != expected_llvm_type) {
            record_failure(
                state,
                ExpressionLoweringFailureReason::call_return_type_mismatch,
                expression.left->text + " returns " + function->second.return_type +
                    ", expected " + std::string(expected_llvm_type)
            );
            return std::nullopt;
        }
        if (function->second.parameter_types.size() != expression.arguments.size()) {
            record_failure(
                state,
                ExpressionLoweringFailureReason::call_arity_mismatch,
                expression.left->text + " expects " +
                    std::to_string(function->second.parameter_types.size()) + " arguments, got " +
                    std::to_string(expression.arguments.size())
            );
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
                if (state.expression_failure.reason == ExpressionLoweringFailureReason::none) {
                    record_failure(
                        state,
                        ExpressionLoweringFailureReason::call_argument_failure,
                        expression.left->text + " argument " + std::to_string(index + 1)
                    );
                }
                return std::nullopt;
            }
            auto promotion = index >= function->second.fixed_abi_parameter_count
                ? c_abi_promotion_for(argument->type)
                : CAbiPromotion::none;
            if (function->second.adapter == CAbiAdapterKind::variadic &&
                promotion == CAbiPromotion::integer_to_i32) {
                auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
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
                auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
                output << "  " << temporary_name << " = fpext float " << argument->value << " to double\n";
                argument = LoweredExpression {
                    .type = "double",
                    .value = std::move(temporary_name),
                    .signedness = IntegerSignedness::not_integer,
                };
            }
            arguments.push_back(std::move(*argument));
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
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

    record_failure(
        state,
        ExpressionLoweringFailureReason::unsupported_expression,
        expression.text
    );
    return std::nullopt;
}

}  // namespace

auto lower_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    ExpressionEmissionContext const& context,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    state.expression_failure = {};
    return lowered_expression(
        expression,
        expected_llvm_type,
        expected_signedness,
        context,
        state,
        output
    );
}

auto infer_expression_type(
    syntax::ExpressionSyntax const& expression,
    ExpressionEmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    return inferred_expression_type(expression, context, state);
}

auto lower_integer_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness
) -> std::optional<LoweredExpression> {
    return lowered_integer_literal(expression, expected_llvm_type, expected_signedness);
}

auto lower_boolean_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type
) -> std::optional<LoweredExpression> {
    return lowered_boolean_literal(expression, expected_llvm_type);
}

auto is_integer_llvm_type(std::string_view type) -> bool {
    return is_integer_llvm_type_impl(type);
}

auto render_expression_lowering_failure(
    ExpressionLoweringFailure const& failure
) -> std::string {
    auto prefix = std::string {};
    switch (failure.reason) {
    case ExpressionLoweringFailureReason::none:
        return {};
    case ExpressionLoweringFailureReason::unsupported_expression:
        prefix = "unsupported expression";
        break;
    case ExpressionLoweringFailureReason::missing_string_constant:
        prefix = "missing lowered string constant";
        break;
    case ExpressionLoweringFailureReason::unknown_name:
        prefix = "unknown lowered name";
        break;
    case ExpressionLoweringFailureReason::type_mismatch:
        prefix = "expression type mismatch";
        break;
    case ExpressionLoweringFailureReason::signedness_mismatch:
        prefix = "expression signedness mismatch";
        break;
    case ExpressionLoweringFailureReason::unsupported_cast:
        prefix = "unsupported cast";
        break;
    case ExpressionLoweringFailureReason::unsupported_operator:
        prefix = "unsupported operator";
        break;
    case ExpressionLoweringFailureReason::cannot_infer_operand_type:
        prefix = "cannot infer operand type";
        break;
    case ExpressionLoweringFailureReason::branch_type_mismatch:
        prefix = "branch type mismatch";
        break;
    case ExpressionLoweringFailureReason::unknown_function:
        prefix = "unknown lowered function";
        break;
    case ExpressionLoweringFailureReason::call_return_type_mismatch:
        prefix = "call return type mismatch";
        break;
    case ExpressionLoweringFailureReason::call_arity_mismatch:
        prefix = "call arity mismatch";
        break;
    case ExpressionLoweringFailureReason::call_argument_failure:
        prefix = "call argument lowering failed";
        break;
    }
    return failure.detail.empty() ? prefix : prefix + ": " + failure.detail;
}

}  // namespace orison::lowering
