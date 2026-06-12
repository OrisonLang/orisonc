#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/merge_plan.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

using EmissionContext = LoweringEmissionContext;

void record_failure(
    LoweringFailures& failures,
    ExpressionLoweringFailureReason reason,
    std::string detail
) {
    if (failures.expression.reason == ExpressionLoweringFailureReason::none) {
        failures.expression = ExpressionLoweringFailure {
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
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto& state = session.state;
    auto& failures = session.failures;
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
                failures,
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
            record_failure(failures, ExpressionLoweringFailureReason::unknown_name, expression.text);
            return std::nullopt;
        }
        if (binding->second.type != expected_llvm_type) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::type_mismatch,
                expression.text + " has LLVM type " + binding->second.type +
                    ", expected " + std::string(expected_llvm_type)
            );
            return std::nullopt;
        }
        if (binding->second.signedness != expected_signedness) {
            record_failure(
                failures,
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
                failures,
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
                failures,
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
            session,
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
            session,
            output
        );
        if (!condition.has_value()) {
            return std::nullopt;
        }

        auto plan = plan_conditional(
            ConditionalPlanKind::ternary,
            next_llvm_block_index(state.next_block_index)
        );
        emit_llvm_conditional_branch(
            output,
            condition->value,
            plan.then_block,
            plan.else_block
        );

        emit_llvm_block_label(output, plan.then_block);
        state.current_block = plan.then_block;
        auto then_value = lowered_expression(
            *expression.right,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        );
        if (!then_value.has_value()) {
            return std::nullopt;
        }
        auto then_exit_block = state.current_block;
        emit_llvm_branch(output, plan.merge_block);

        emit_llvm_block_label(output, plan.else_block);
        state.current_block = plan.else_block;
        auto else_value = lowered_expression(
            *expression.alternate,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        );
        if (!else_value.has_value()) {
            return std::nullopt;
        }
        auto else_exit_block = state.current_block;
        auto merge_inputs = std::array {
            BranchMergeInput {
                .value = &*then_value,
                .block = then_exit_block,
            },
            BranchMergeInput {
                .value = &*else_value,
                .block = else_exit_block,
            },
        };
        auto merge = plan_branch_merge(merge_inputs);
        if (!merge.plan.has_value()) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::branch_type_mismatch,
                "ternary branches"
            );
            return std::nullopt;
        }
        emit_llvm_branch(output, plan.merge_block);

        emit_llvm_block_label(output, plan.merge_block);
        state.current_block = plan.merge_block;
        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        emit_llvm_phi(
            output,
            temporary_name,
            merge.plan->result_type.type,
            merge.plan->incoming
        );
        return LoweredExpression {
            .type = merge.plan->result_type.type,
            .value = std::move(temporary_name),
            .signedness = merge.plan->result_type.signedness,
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
                    session,
                    output
                );
                auto right = lowered_expression(
                    *expression.right,
                    "i1",
                    IntegerSignedness::not_integer,
                    context,
                    session,
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
                    failures,
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
                    session,
                    output
                );
                auto right = lowered_expression(
                    *expression.right,
                    operand_type->type,
                    operand_type->signedness,
                    context,
                    session,
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
                failures,
                ExpressionLoweringFailureReason::unsupported_operator,
                expression.text
            );
            return std::nullopt;
        }
        auto left = lowered_expression(
            *expression.left,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        );
        auto right = lowered_expression(
            *expression.right,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        );
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
                failures,
                ExpressionLoweringFailureReason::unknown_function,
                expression.left->text
            );
            return std::nullopt;
        }
        if (function->second.return_type != expected_llvm_type) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::call_return_type_mismatch,
                expression.left->text + " returns " + function->second.return_type +
                    ", expected " + std::string(expected_llvm_type)
            );
            return std::nullopt;
        }
        if (function->second.parameter_types.size() != expression.arguments.size()) {
            record_failure(
                failures,
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
                session,
                output
            );
            if (!argument.has_value()) {
                if (failures.expression.reason == ExpressionLoweringFailureReason::none) {
                    record_failure(
                        failures,
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
        failures,
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
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto& failures = session.failures;
    failures.expression = {};
    return lowered_expression(
        expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
}

auto infer_expression_type(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
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

}  // namespace orison::lowering
