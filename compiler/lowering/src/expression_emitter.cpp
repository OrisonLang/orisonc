#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/conditional_emitter.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <optional>
#include <span>
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

struct ResolvedMemberCall {
    MemberCallReceiverInference receiver;
    LoweredMethodLookup method;
};

auto resolve_member_call(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> ResolvedMemberCall {
    auto receiver = infer_member_call_receiver(expression, state);
    if (receiver.result != MemberCallReceiverInferenceResult::found) {
        return {.receiver = std::move(receiver)};
    }

    auto method = find_lowered_method_signature(
        context.lowering,
        receiver.receiver_type_name,
        receiver.method_name
    );
    return ResolvedMemberCall {
        .receiver = std::move(receiver),
        .method = std::move(method),
    };
}

auto inferred_expression_type(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto binding = state.immutable_bindings.find(expression.text);
        if (binding != state.immutable_bindings.end()) {
            return LoweredType {
                .type = binding->second.type,
                .signedness = binding->second.signedness,
            };
        }
        auto mutable_binding = state.mutable_bindings.find(expression.text);
        return mutable_binding == state.mutable_bindings.end()
            ? std::nullopt
            : std::optional<LoweredType> {mutable_binding->second.type};
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
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "Pointer" &&
        expression.arguments.size() == 1) {
        return LoweredType {
            .type = "ptr",
            .signedness = IntegerSignedness::not_integer,
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

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::member_access) {
        auto resolved = resolve_member_call(expression, context, state);
        if (resolved.receiver.result != MemberCallReceiverInferenceResult::found ||
            resolved.method.result != LoweredMethodLookupResult::found ||
            resolved.method.method == nullptr ||
            !has_supported_function_signature_types(resolved.method.method->signature)) {
            return std::nullopt;
        }

        return LoweredType {
            .type = resolved.method.method->signature.return_type,
            .signedness = resolved.method.method->signature.return_signedness,
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
            auto mutable_binding = state.mutable_bindings.find(expression.text);
            if (mutable_binding == state.mutable_bindings.end()) {
                record_failure(failures, ExpressionLoweringFailureReason::unknown_name, expression.text);
                return std::nullopt;
            }
            if (mutable_binding->second.type.type != expected_llvm_type) {
                record_failure(
                    failures,
                    ExpressionLoweringFailureReason::type_mismatch,
                    expression.text + " has LLVM type " + mutable_binding->second.type.type +
                        ", expected " + std::string(expected_llvm_type)
                );
                return std::nullopt;
            }
            if (mutable_binding->second.type.signedness != expected_signedness) {
                record_failure(
                    failures,
                    ExpressionLoweringFailureReason::signedness_mismatch,
                    expression.text
                );
                return std::nullopt;
            }
            auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
            output << "  " << temporary_name << " = load " << mutable_binding->second.type.type
                   << ", ptr " << mutable_binding->second.storage << "\n";
            return LoweredExpression {
                .type = mutable_binding->second.type.type,
                .value = std::move(temporary_name),
                .signedness = mutable_binding->second.type.signedness,
            };
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
        struct ArmContext {
            syntax::ExpressionSyntax const& then_expression;
            syntax::ExpressionSyntax const& else_expression;
            std::string_view expected_llvm_type;
            IntegerSignedness expected_signedness;
            EmissionContext const& context;
            FunctionLoweringSession& session;
            std::ostringstream& output;
        };
        auto arm_context = ArmContext {
            .then_expression = *expression.right,
            .else_expression = *expression.alternate,
            .expected_llvm_type = expected_llvm_type,
            .expected_signedness = expected_signedness,
            .context = context,
            .session = session,
            .output = output,
        };
        auto result = emit_conditional_value(
            plan,
            condition->value,
            state,
            output,
            ConditionalLoweringCallbacks {
                .context = &arm_context,
                .lower_then = [](void* opaque) {
                    auto& arm = *static_cast<ArmContext*>(opaque);
                    return lowered_expression(
                        arm.then_expression,
                        arm.expected_llvm_type,
                        arm.expected_signedness,
                        arm.context,
                        arm.session,
                        arm.output
                    );
                },
                .lower_else = [](void* opaque) {
                    auto& arm = *static_cast<ArmContext*>(opaque);
                    return lowered_expression(
                        arm.else_expression,
                        arm.expected_llvm_type,
                        arm.expected_signedness,
                        arm.context,
                        arm.session,
                        arm.output
                    );
                },
            }
        );
        if (result.failure == ConditionalEmissionFailure::branch_mismatch) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::branch_type_mismatch,
                "ternary branches"
            );
            return std::nullopt;
        }
        return result.value;
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
        expression.left->kind == syntax::ExpressionKind::null_safe_member_access) {
        record_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe member call lowering is not yet supported"
        );
        return std::nullopt;
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "Pointer") {
        if (expected_llvm_type != "ptr") {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "Pointer construction"
            );
            return std::nullopt;
        }
        if (expression.arguments.size() != 1) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "Pointer construction"
            );
            return std::nullopt;
        }

        auto source = lowered_expression(
            expression.arguments.front(),
            "i64",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!source.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        output << "  " << temporary_name << " = inttoptr i64 " << source->value << " to ptr\n";
        return LoweredExpression {
            .type = "ptr",
            .value = std::move(temporary_name),
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::member_access) {
        auto resolved = resolve_member_call(expression, context, state);
        auto const receiver_name = expression.left->left != nullptr ? expression.left->left->text : std::string {};
        auto const target_name = resolved.receiver.receiver_type_name + "." + resolved.receiver.method_name;
        if (resolved.receiver.result == MemberCallReceiverInferenceResult::unsupported_shape) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "member call receiver shape"
            );
            return std::nullopt;
        }
        if (resolved.receiver.result == MemberCallReceiverInferenceResult::not_found) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unknown_member_call_receiver,
                receiver_name
            );
            return std::nullopt;
        }
        if (resolved.method.result == LoweredMethodLookupResult::not_found) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unknown_member_call_target,
                target_name
            );
            return std::nullopt;
        }
        if (resolved.method.result == LoweredMethodLookupResult::ambiguous) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::ambiguous_member_call_target,
                target_name
            );
            return std::nullopt;
        }
        if (resolved.method.method == nullptr ||
            !has_supported_function_signature_types(resolved.method.method->signature)) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "member call target is not lowerable: " + target_name
            );
            return std::nullopt;
        }

        auto const& method = resolved.method.method->signature;
        if (method.return_type != expected_llvm_type) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::call_return_type_mismatch,
                target_name + " returns " + method.return_type + ", expected " +
                    std::string(expected_llvm_type)
            );
            return std::nullopt;
        }

        auto const expected_argument_count = method.parameter_types.size() - 1;
        if (expected_argument_count != expression.arguments.size()) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::call_arity_mismatch,
                target_name + " expects " + std::to_string(expected_argument_count) +
                    " arguments, got " + std::to_string(expression.arguments.size())
            );
            return std::nullopt;
        }

        auto arguments = lower_member_call_arguments(
            *expression.left->left,
            std::span<syntax::ExpressionSyntax const>(
                expression.arguments.data(),
                expression.arguments.size()
            ),
            method,
            context,
            session,
            output
        );
        if (!arguments.has_value()) {
            if (failures.expression.reason == ExpressionLoweringFailureReason::none) {
                record_failure(
                    failures,
                    ExpressionLoweringFailureReason::call_argument_failure,
                    target_name
                );
            }
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        return emit_value_call(std::move(temporary_name), method, *arguments, output);
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

        auto arguments = lower_call_arguments(expression, function->second, context, session, output);
        if (!arguments.has_value()) {
            if (failures.expression.reason == ExpressionLoweringFailureReason::none) {
                record_failure(
                    failures,
                    ExpressionLoweringFailureReason::call_argument_failure,
                    expression.left->text
                );
            }
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        return emit_value_call(std::move(temporary_name), function->second, *arguments, output);
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
