#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/conditional_emitter.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/concurrency_emitter.hpp"
#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_failure_lifecycle.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/maybe_value_emitter.hpp"
#include "orison/lowering/null_safe_plan.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
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

auto digit_value_for_base(char character, int base) -> std::optional<std::uint64_t> {
    auto value = std::optional<std::uint64_t> {};
    if (character >= '0' && character <= '9') {
        value = static_cast<std::uint64_t>(character - '0');
    } else if (character >= 'a' && character <= 'f') {
        value = static_cast<std::uint64_t>(character - 'a' + 10);
    } else if (character >= 'A' && character <= 'F') {
        value = static_cast<std::uint64_t>(character - 'A' + 10);
    }

    if (!value.has_value() || *value >= static_cast<std::uint64_t>(base)) {
        return std::nullopt;
    }
    return value;
}

auto normalized_integer_literal_text(std::string_view text) -> std::optional<std::string> {
    auto base = 10;
    auto digits = text;
    if (digits.starts_with("0x") || digits.starts_with("0X")) {
        base = 16;
        digits.remove_prefix(2);
    } else if (digits.starts_with("0b") || digits.starts_with("0B")) {
        base = 2;
        digits.remove_prefix(2);
    }
    if (digits.empty()) {
        return std::nullopt;
    }

    if (base == 10) {
        for (auto character : digits) {
            if (std::isdigit(static_cast<unsigned char>(character)) == 0) {
                return std::nullopt;
            }
        }
        return std::string {text};
    }

    auto value = std::uint64_t {0};
    for (auto character : digits) {
        auto digit = digit_value_for_base(character, base);
        if (!digit.has_value()) {
            return std::nullopt;
        }
        value = (value * static_cast<std::uint64_t>(base)) + *digit;
    }
    return std::to_string(value);
}

auto lowered_integer_literal(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::integer_literal) {
        return std::nullopt;
    }
    auto value = normalized_integer_literal_text(expression.text);
    if (!value.has_value()) {
        return std::nullopt;
    }

    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = std::move(*value),
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

auto inferred_expression_type(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType>;

auto signedness_for_expected_array_element(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> IntegerSignedness {
    auto inferred = inferred_expression_type(expression, context, state);
    if (inferred.has_value() && inferred->type == expected_llvm_type) {
        return inferred->signedness;
    }
    return IntegerSignedness::not_integer;
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
    if (operator_text == "%") {
        if (signedness == IntegerSignedness::not_integer) {
            return std::nullopt;
        }
        return signedness == IntegerSignedness::signed_integer ? "srem" : "urem";
    }
    if (operator_text == "bit_and") {
        return "and";
    }
    if (operator_text == "bit_or") {
        return "or";
    }
    if (operator_text == "bit_xor") {
        return "xor";
    }
    if (operator_text == "shift_left") {
        return "shl";
    }
    if (operator_text == "shift_right") {
        return signedness == IntegerSignedness::signed_integer ? "ashr" : "lshr";
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

auto llvm_boolean_comparison_predicate_for(
    std::string_view operator_text
) -> std::optional<std::string_view> {
    if (operator_text == "==") {
        return "eq";
    }
    if (operator_text == "!=") {
        return "ne";
    }
    return std::nullopt;
}

auto is_integer_llvm_type_impl(std::string_view type) -> bool {
    return type == "i8" || type == "i16" || type == "i32" || type == "i64";
}

auto is_decimal_integer_text(std::string_view text) -> bool {
    return !text.empty() && std::ranges::all_of(text, [](char value) {
        return std::isdigit(static_cast<unsigned char>(value)) != 0;
    });
}

auto lowered_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name = std::nullopt
) -> std::optional<LoweredExpression>;

auto inferred_expression_type(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType>;

auto is_low_level_intrinsic_name(std::string_view name) -> bool {
    return name == "address_of" || name == "raw_read" || name == "raw_write" ||
           name == "raw_offset" || name == "volatile_read" || name == "volatile_write";
}

auto is_write_intrinsic_name(std::string_view name) -> bool {
    return name == "raw_write" || name == "volatile_write";
}

auto is_concurrency_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::task ||
           expression.kind == syntax::ExpressionKind::thread ||
           (expression.kind == syntax::ExpressionKind::unary && expression.text == "await");
}

auto pointer_pointee_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"Pointer<"};
    if (!type_name.starts_with(prefix) || !type_name.ends_with(">") ||
        type_name.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    return std::string(type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1));
}

auto lowered_type_for_source_type_name(std::string_view source_type_name) -> std::optional<LoweredType> {
    auto type = syntax::TypeSyntax {
        .name = std::string(source_type_name),
    };
    auto lowered = llvm_type_for(type);
    if (!lowered.has_value() || *lowered == "void") {
        return std::nullopt;
    }

    return LoweredType {
        .type = std::string(*lowered),
        .signedness = integer_signedness_for(type),
    };
}

auto source_type_name_for_expression(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    return source_type_name_for_expression(expression, context.lowering, state);
}

auto pointee_lowered_type_for_pointer_expression(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    auto source_type = source_type_name_for_expression(expression, context, state);
    if (!source_type.has_value()) {
        return std::nullopt;
    }

    auto pointee_source_type = pointer_pointee_source_type_name(*source_type);
    return pointee_source_type.has_value()
        ? lowered_type_for_source_type_name(*pointee_source_type)
        : std::nullopt;
}

struct ResolvedMemberCall {
    MemberCallReceiverInference receiver;
    LoweredMethodLookup method;
};

struct NullSafeIncoming {
    std::string value;
    std::string block;
};

auto resolve_member_call(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringState const& state
) -> ResolvedMemberCall {
    auto receiver = infer_member_call_receiver(expression, context.lowering, state);
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

    if ((expression.kind == syntax::ExpressionKind::member_access ||
         expression.kind == syntax::ExpressionKind::index_access) &&
        expression.left != nullptr) {
        auto source_type = source_type_name_for_expression(expression, context, state);
        if (!source_type.has_value()) {
            return std::nullopt;
        }
        return lowered_type_for_source_type_name(*source_type, context.lowering);
    }

    if (expression.kind == syntax::ExpressionKind::cast) {
        return lowered_type_for_source_type_name(expression.text, context.lowering);
    }

    if (expression.kind == syntax::ExpressionKind::unary && expression.left != nullptr) {
        if (expression.text == "not") {
            return LoweredType {
                .type = "i1",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (expression.text == "bit_not") {
            auto operand = inferred_expression_type(*expression.left, context, state);
            if (operand.has_value() && is_integer_llvm_type_impl(operand->type) &&
                operand->signedness != IntegerSignedness::not_integer) {
                return operand;
            }
        }
    }

    if (expression.kind == syntax::ExpressionKind::binary && expression.left != nullptr &&
        expression.right != nullptr) {
        if (expression.text == "and" || expression.text == "or") {
            return LoweredType {
                .type = "i1",
                .signedness = IntegerSignedness::not_integer,
            };
        }

        auto left = inferred_expression_type(*expression.left, context, state);
        auto right = inferred_expression_type(*expression.right, context, state);
        if (!left.has_value() || !right.has_value() || left->type != right->type ||
            left->signedness != right->signedness) {
            return std::nullopt;
        }

        if (left->type == "i1" && left->signedness == IntegerSignedness::not_integer &&
            llvm_boolean_comparison_predicate_for(expression.text).has_value()) {
            return LoweredType {
                .type = "i1",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (left->signedness != IntegerSignedness::not_integer &&
            llvm_integer_comparison_predicate_for(expression.text, left->signedness).has_value()) {
            return LoweredType {
                .type = "i1",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (llvm_binary_instruction_for(expression.text, left->signedness).has_value()) {
            return left;
        }
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
        auto record = context.lowering.records.find(expression.left->text);
        if (record != context.lowering.records.end()) {
            return LoweredType {
                .type = record->second.llvm_type_name,
                .signedness = IntegerSignedness::not_integer,
            };
        }

        auto const& intrinsic_name = expression.left->text;
        if (is_write_intrinsic_name(intrinsic_name)) {
            return LoweredType {
                .type = "void",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (intrinsic_name == "address_of") {
            return LoweredType {
                .type = "i64",
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (intrinsic_name == "raw_offset" && !expression.arguments.empty()) {
            auto source_type = inferred_expression_type(expression.arguments.front(), context, state);
            if (source_type.has_value() && (source_type->type == "ptr" || source_type->type == "i64")) {
                return source_type;
            }
        }
        if ((intrinsic_name == "raw_read" || intrinsic_name == "volatile_read") &&
            !expression.arguments.empty()) {
            auto pointee_type =
                pointee_lowered_type_for_pointer_expression(expression.arguments.front(), context, state);
            if (pointee_type.has_value()) {
                return pointee_type;
            }
        }

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
        if (expression.left->text == "join" &&
            expression.left->left != nullptr &&
            expression.left->left->kind == syntax::ExpressionKind::name) {
            auto thread_binding = state.thread_bindings.find(expression.left->left->text);
            if (thread_binding != state.thread_bindings.end()) {
                return thread_binding->second.result_type;
            }
        }

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

    if (expression.kind == syntax::ExpressionKind::unary &&
        expression.text == "await" &&
        expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        auto task_binding = state.task_bindings.find(expression.left->text);
        if (task_binding != state.task_bindings.end()) {
            return task_binding->second.result_type;
        }
    }

    if (expression.kind == syntax::ExpressionKind::array_literal) {
        if (expression.arguments.empty()) {
            return std::nullopt;
        }

        auto element_type = inferred_expression_type(expression.arguments.front(), context, state);
        if (!element_type.has_value()) {
            return std::nullopt;
        }
        for (auto index = std::size_t {1}; index < expression.arguments.size(); ++index) {
            auto next_type = inferred_expression_type(expression.arguments[index], context, state);
            if (!next_type.has_value() || next_type->type != element_type->type ||
                next_type->signedness != element_type->signedness) {
                return std::nullopt;
            }
        }

        return LoweredType {
            .type = "[" + std::to_string(expression.arguments.size()) + " x " + element_type->type + "]",
            .signedness = IntegerSignedness::not_integer,
        };
    }

    return std::nullopt;
}

auto lower_address_to_pointer(
    LoweredExpression address,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = inttoptr i64 " << address.value << " to ptr\n";
    return LoweredExpression {
        .type = "ptr",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_pointer_to_address(
    std::string pointer_value,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = ptrtoint ptr " << pointer_value << " to i64\n";
    return LoweredExpression {
        .type = "i64",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto emit_pointer_load(
    std::string_view result_type,
    IntegerSignedness result_signedness,
    std::string_view pointer_value,
    bool is_volatile,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = load ";
    if (is_volatile) {
        output << "volatile ";
    }
    output << result_type << ", ptr " << pointer_value << "\n";
    return LoweredExpression {
        .type = std::string(result_type),
        .value = std::move(temporary_name),
        .signedness = result_signedness,
    };
}

void emit_pointer_store(
    std::string_view value_type,
    std::string_view value,
    std::string_view pointer_value,
    bool is_volatile,
    std::ostringstream& output
) {
    output << "  store ";
    if (is_volatile) {
        output << "volatile ";
    }
    output << value_type << " " << value << ", ptr " << pointer_value << "\n";
}

auto emit_address_offset(
    std::string_view address_value,
    std::string_view offset_value,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = add i64 " << address_value << ", " << offset_value << "\n";
    return LoweredExpression {
        .type = "i64",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto emit_pointer_offset(
    LoweredType const& pointee_type,
    std::string_view pointer_value,
    std::string_view offset_value,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = getelementptr " << pointee_type.type << ", ptr "
           << pointer_value << ", i64 " << offset_value << "\n";
    return LoweredExpression {
        .type = "ptr",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_pointer_operand(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto inferred = inferred_expression_type(expression, context, session.state);
    if (inferred.has_value() && inferred->type == "ptr") {
        return lowered_expression(
            expression,
            "ptr",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
    }

    auto source_type = source_type_name_for_expression(expression, context, session.state);
    if (inferred.has_value() && inferred->type == "i64" &&
        source_type.has_value() && *source_type == "Address") {
        auto address = lowered_expression(
            expression,
            "i64",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!address.has_value()) {
            return std::nullopt;
        }
        return lower_address_to_pointer(std::move(*address), session, output);
    }

    return lowered_expression(
        expression,
        "ptr",
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
}

auto lower_address_operand(
    syntax::ExpressionSyntax const& expression,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto inferred = inferred_expression_type(expression, context, session.state);
    if (inferred.has_value() && inferred->type == "ptr") {
        auto pointer = lowered_expression(
            expression,
            "ptr",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!pointer.has_value()) {
            return std::nullopt;
        }

        return lower_pointer_to_address(std::move(pointer->value), session, output);
    }

    return lowered_expression(
        expression,
        "i64",
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
}

auto lower_array_literal_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::array_literal) {
        return std::nullopt;
    }

    auto array_type = parse_llvm_array_type(expected_llvm_type);
    if (!array_type.has_value()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::type_mismatch,
            "array literal expected LLVM array type " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }
    if (expression.arguments.size() != array_type->length) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "array literal element count"
        );
        return std::nullopt;
    }
    if (array_type->length == 0) {
        return LoweredExpression {
            .type = std::string(expected_llvm_type),
            .value = "zeroinitializer",
            .signedness = IntegerSignedness::not_integer,
        };
    }

    auto element_source_type_name = expected_source_type_name.has_value()
        ? array_element_source_type_name(*expected_source_type_name)
        : std::optional<std::string> {};
    auto element_expected_source_type = element_source_type_name.has_value()
        ? std::optional<std::string_view> {*element_source_type_name}
        : std::optional<std::string_view> {};

    auto aggregate_value = std::string {"undef"};
    for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
        auto const& element = expression.arguments[index];
        auto lowered_element = lowered_expression(
            element,
            array_type->element_type,
            signedness_for_expected_array_element(
                element,
                array_type->element_type,
                context,
                session.state
            ),
            context,
            session,
            output,
            element_expected_source_type
        );
        if (!lowered_element.has_value()) {
            return std::nullopt;
        }

        auto aggregate_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << aggregate_name << " = insertvalue " << expected_llvm_type << " "
               << aggregate_value << ", " << array_type->element_type << " "
               << lowered_element->value << ", " << index << "\n";
        aggregate_value = std::move(aggregate_name);
    }

    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = std::move(aggregate_value),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_record_constructor_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.left == nullptr || expression.left->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto layout = context.lowering.records.find(expression.left->text);
    if (layout == context.lowering.records.end()) {
        return std::nullopt;
    }
    if (layout->second.llvm_type_name != expected_llvm_type) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::type_mismatch,
            expression.left->text + " has LLVM type " + layout->second.llvm_type_name +
                ", expected " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }
    if (layout->second.fields.size() != expression.arguments.size()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::call_arity_mismatch,
            expression.left->text + " expects " + std::to_string(layout->second.fields.size()) +
                " arguments, got " + std::to_string(expression.arguments.size())
        );
        return std::nullopt;
    }
    if (layout->second.fields.empty()) {
        return LoweredExpression {
            .type = layout->second.llvm_type_name,
            .value = "zeroinitializer",
            .signedness = IntegerSignedness::not_integer,
        };
    }

    auto aggregate_value = std::string {"undef"};
    for (auto index = std::size_t {0}; index < layout->second.fields.size(); ++index) {
        auto const& field = layout->second.fields[index];
        if (field.llvm_type.empty() || field.llvm_type == "void") {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "record constructor field layout"
            );
            return std::nullopt;
        }

        auto lowered_field = lowered_expression(
            expression.arguments[index],
            field.llvm_type,
            integer_signedness_for(syntax::TypeSyntax {.name = field.source_type_name}),
            context,
            session,
            output,
            field.source_type_name.empty()
                ? std::optional<std::string_view> {}
                : std::optional<std::string_view> {field.source_type_name}
        );
        if (!lowered_field.has_value()) {
            return std::nullopt;
        }

        auto aggregate_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << aggregate_name << " = insertvalue " << layout->second.llvm_type_name << " "
               << aggregate_value << ", " << field.llvm_type << " " << lowered_field->value << ", "
               << index << "\n";
        aggregate_value = std::move(aggregate_name);
    }

    return LoweredExpression {
        .type = layout->second.llvm_type_name,
        .value = std::move(aggregate_value),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto find_choice_variant(
    LoweredChoiceLayout const& layout,
    std::string_view variant_name
) -> LoweredChoiceVariant const* {
    for (auto const& variant : layout.variants) {
        if (variant.name == variant_name) {
            return &variant;
        }
    }
    return nullptr;
}

struct ChoiceConstructorLayoutLookup {
    LoweredChoiceLayout const* layout = nullptr;
    bool ambiguous = false;
};

auto choice_constructor_layout_for(
    std::string_view variant_name,
    std::string_view expected_llvm_type,
    LoweringContext const& context,
    std::optional<std::string_view> expected_source_type_name
) -> ChoiceConstructorLayoutLookup {
    auto const* match = static_cast<LoweredChoiceLayout const*>(nullptr);
    for (auto const& [choice_name, layout] : context.choices) {
        (void)choice_name;
        if (layout.llvm_type_name != expected_llvm_type || find_choice_variant(layout, variant_name) == nullptr) {
            continue;
        }
        if (expected_source_type_name.has_value() && layout.source_type_name != *expected_source_type_name) {
            continue;
        }
        if (match != nullptr) {
            return ChoiceConstructorLayoutLookup {
                .ambiguous = true,
            };
        }
        match = &layout;
    }
    return ChoiceConstructorLayoutLookup {
        .layout = match,
    };
}

auto lower_choice_constructor_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto const* constructor_name = static_cast<std::string const*>(nullptr);
    auto const* arguments = static_cast<std::vector<syntax::ExpressionSyntax> const*>(nullptr);
    if (expression.kind == syntax::ExpressionKind::name) {
        constructor_name = &expression.text;
    } else if (expression.kind == syntax::ExpressionKind::call &&
               expression.left != nullptr &&
               expression.left->kind == syntax::ExpressionKind::name) {
        constructor_name = &expression.left->text;
        arguments = &expression.arguments;
    }
    if (constructor_name == nullptr) {
        return std::nullopt;
    }

    auto layout_lookup = choice_constructor_layout_for(
        *constructor_name,
        expected_llvm_type,
        context.lowering,
        expected_source_type_name
    );
    if (layout_lookup.ambiguous) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "choice constructor '" + *constructor_name +
                "' is ambiguous for lowered choice ABI type '" + std::string(expected_llvm_type) + "'"
        );
        return std::nullopt;
    }
    auto const* layout = layout_lookup.layout;
    if (layout == nullptr) {
        return std::nullopt;
    }
    auto const* variant = find_choice_variant(*layout, *constructor_name);
    if (variant == nullptr) {
        return std::nullopt;
    }

    auto const argument_count = arguments == nullptr ? std::size_t {0} : arguments->size();
    if (argument_count != variant->payloads.size()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::call_arity_mismatch,
            *constructor_name + " expects " + std::to_string(variant->payloads.size()) +
                " arguments, got " + std::to_string(argument_count)
        );
        return std::nullopt;
    }

    if (layout->llvm_type_name == "i32") {
        return LoweredExpression {
            .type = layout->llvm_type_name,
            .value = std::to_string(variant->tag),
            .signedness = IntegerSignedness::unsigned_integer,
        };
    }

    auto tag_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << tag_name << " = insertvalue " << layout->llvm_type_name << " undef, i32 "
           << variant->tag << ", 0\n";
    auto aggregate_value = std::move(tag_name);
    if (!variant->payloads.empty()) {
        auto const& payload = variant->payloads.front();
        auto lowered_payload = lowered_expression(
            arguments->front(),
            payload.llvm_type,
            integer_signedness_for(syntax::TypeSyntax {.name = payload.source_type_name}),
            context,
            session,
            output
        );
        if (!lowered_payload.has_value()) {
            return std::nullopt;
        }
        auto payload_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << payload_name << " = insertvalue " << layout->llvm_type_name << " "
               << aggregate_value << ", " << payload.llvm_type << " " << lowered_payload->value
               << ", 1\n";
        aggregate_value = std::move(payload_name);
    }

    return LoweredExpression {
        .type = layout->llvm_type_name,
        .value = std::move(aggregate_value),
        .signedness = IntegerSignedness::not_integer,
    };
}

struct AggregateAddressBase {
    std::string pointer_value;
    std::string source_type_name;
};

auto resolve_aggregate_address_base(
    syntax::ExpressionSyntax const& base_expression,
    std::string_view base_source_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<AggregateAddressBase> {
    if (auto record_name = pointer_pointee_source_type_name(base_source_type)) {
        auto base_pointer = lower_pointer_operand(base_expression, context, session, output);
        if (!base_pointer.has_value()) {
            return std::nullopt;
        }
        return AggregateAddressBase {
            .pointer_value = std::move(base_pointer->value),
            .source_type_name = std::move(*record_name),
        };
    }

    if (base_expression.kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto binding = session.state.mutable_bindings.find(base_expression.text);
    if (binding == session.state.mutable_bindings.end()) {
        return std::nullopt;
    }
    return AggregateAddressBase {
        .pointer_value = binding->second.storage,
        .source_type_name = std::string(base_source_type),
    };
}

auto advance_address_of_aggregate_path_step(
    AggregatePathCursor& cursor,
    AggregatePathStep const& step,
    LoweringEmissionContext const& context,
    LoweringFailures& failures,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> bool {
    if (step.kind == AggregatePathStepKind::member) {
        auto result = advance_aggregate_path_member_with_temporary(
            cursor,
            step.field_name,
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error != AggregatePathError::none) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "address_of field layout"
            );
            return false;
        }
        return true;
    }

    if (step.index_expression == nullptr) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of indexed field layout"
        );
        return false;
    }

    auto lowered_index = lowered_expression(
        *step.index_expression,
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!lowered_index.has_value()) {
        return false;
    }

    auto result = advance_aggregate_path_index_with_temporary(
        cursor,
        lowered_index->value,
        context.lowering,
        session.state.next_temporary_index,
        output
    );
    if (result.error != AggregatePathError::none) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            result.error == AggregatePathError::unsupported_element_source_type
                ? "address_of indexed source type"
                : "address_of indexed field layout"
        );
        return false;
    }
    return true;
}

auto lower_pointer_record_field_address(
    syntax::ExpressionSyntax const& operand,
    LoweringEmissionContext const& context,
    LoweringFailures& failures,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto path = collect_aggregate_path(operand);
    if (path.steps.empty() || path.base_expression == nullptr) {
        return std::nullopt;
    }

    auto const& base_expression = *path.base_expression;
    auto base_source_type = source_type_name_for_expression(base_expression, context, session.state);
    if (!base_source_type.has_value()) {
        return std::nullopt;
    }

    auto base = resolve_aggregate_address_base(
        base_expression,
        *base_source_type,
        context,
        session,
        output
    );
    if (!base.has_value()) {
        return std::nullopt;
    }

    auto cursor = initialize_aggregate_path_cursor(
        std::move(base->pointer_value),
        std::move(base->source_type_name),
        context.lowering
    );
    if (!cursor.has_value()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of field source record layout"
        );
        return std::nullopt;
    }

    for (auto const& step : path.steps) {
        if (!advance_address_of_aggregate_path_step(
                *cursor,
                step,
                context,
                failures,
                session,
                output
            )) {
            return std::nullopt;
        }
    }

    return lower_pointer_to_address(cursor->pointer, session, output);
}

auto lower_address_of_intrinsic(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    LoweringFailures& failures,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expected_llvm_type != "i64" || expression.arguments.size() != 1) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of"
        );
        return std::nullopt;
    }

    auto const& operand = expression.arguments.front();
    if (auto field_address = lower_pointer_record_field_address(
            operand,
            context,
            failures,
            session,
            output
        )) {
        return field_address;
    }

    if (operand.kind != syntax::ExpressionKind::name) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of currently only lowers mutable local names and aggregate fields"
        );
        return std::nullopt;
    }

    auto binding = session.state.mutable_bindings.find(operand.text);
    if (binding == session.state.mutable_bindings.end()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of currently only lowers mutable local names and aggregate fields"
        );
        return std::nullopt;
    }

    return lower_pointer_to_address(binding->second.storage, session, output);
}

auto lower_pointer_constructor_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expected_llvm_type != "ptr") {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "Pointer construction"
        );
        return std::nullopt;
    }
    if (expression.arguments.size() != 1) {
        record_expression_lowering_failure(
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

    return lower_address_to_pointer(std::move(*source), session, output);
}

auto lower_raw_offset_intrinsic(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.arguments.size() != 2) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "raw_offset"
        );
        return std::nullopt;
    }

    auto offset = lowered_expression(
        expression.arguments[1],
        "i64",
        IntegerSignedness::unsigned_integer,
        context,
        session,
        output
    );
    if (!offset.has_value()) {
        return std::nullopt;
    }

    if (expected_llvm_type == "i64") {
        auto address = lower_address_operand(expression.arguments.front(), context, session, output);
        if (!address.has_value()) {
            return std::nullopt;
        }

        return emit_address_offset(address->value, offset->value, session, output);
    }

    if (expected_llvm_type != "ptr") {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::type_mismatch,
            "raw_offset expected ptr or i64 result"
        );
        return std::nullopt;
    }

    auto pointee_type =
        pointee_lowered_type_for_pointer_expression(expression.arguments.front(), context, session.state);
    if (!pointee_type.has_value()) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "raw_offset requires a known Pointer<T> source"
        );
        return std::nullopt;
    }

    auto pointer = lower_pointer_operand(expression.arguments.front(), context, session, output);
    if (!pointer.has_value()) {
        return std::nullopt;
    }

    return emit_pointer_offset(*pointee_type, pointer->value, offset->value, session, output);
}

auto lower_read_intrinsic(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    bool is_volatile,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expected_llvm_type == "void" || expression.arguments.size() != 1) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            is_volatile ? "volatile_read" : "raw_read"
        );
        return std::nullopt;
    }

    auto pointer = lower_pointer_operand(expression.arguments.front(), context, session, output);
    if (!pointer.has_value()) {
        return std::nullopt;
    }

    return emit_pointer_load(
        expected_llvm_type,
        expected_signedness,
        pointer->value,
        is_volatile,
        session,
        output
    );
}

auto lower_write_intrinsic(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    bool is_volatile,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expected_llvm_type != "void" || expression.arguments.size() != 2) {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            is_volatile ? "volatile_write" : "raw_write"
        );
        return std::nullopt;
    }

    auto value_type =
        pointee_lowered_type_for_pointer_expression(expression.arguments.front(), context, session.state);
    if (!value_type.has_value()) {
        value_type = inferred_expression_type(expression.arguments[1], context, session.state);
    }
    if (!value_type.has_value() || value_type->type.empty() || value_type->type == "void") {
        record_expression_lowering_failure(
            failures,
            ExpressionLoweringFailureReason::cannot_infer_operand_type,
            is_volatile ? "volatile_write value" : "raw_write value"
        );
        return std::nullopt;
    }

    auto pointer = lower_pointer_operand(expression.arguments.front(), context, session, output);
    if (!pointer.has_value()) {
        return std::nullopt;
    }
    auto value = lowered_expression(
        expression.arguments[1],
        value_type->type,
        value_type->signedness,
        context,
        session,
        output
    );
    if (!value.has_value()) {
        return std::nullopt;
    }

    emit_pointer_store(value_type->type, value->value, pointer->value, is_volatile, output);
    return LoweredExpression {
        .type = "void",
        .value = "",
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_aggregate_path_read_from_storage(
    syntax::ExpressionSyntax const& expression,
    AggregatePath const& path,
    std::string storage,
    std::string_view base_source_type_name,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto inferred = inferred_expression_type(expression, context, session.state);
    if (!inferred.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            expression.text
        );
        return std::nullopt;
    }
    if (inferred->type != expected_llvm_type) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::type_mismatch,
            expression.text
        );
        return std::nullopt;
    }
    if (inferred->signedness != expected_signedness) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::signedness_mismatch,
            expression.text
        );
        return std::nullopt;
    }

    auto cursor = initialize_aggregate_path_cursor(
        std::move(storage),
        std::string(base_source_type_name),
        context.lowering
    );
    if (!cursor.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            expression.text
        );
        return std::nullopt;
    }

    for (auto const& step : path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto result = advance_aggregate_path_member_with_temporary(
                *cursor,
                step.field_name,
                context.lowering,
                session.state.next_temporary_index,
                output
            );
            if (result.error != AggregatePathError::none) {
                record_expression_lowering_failure(
                    session.failures,
                    ExpressionLoweringFailureReason::unsupported_expression,
                    expression.text
                );
                return std::nullopt;
            }
            continue;
        }

        if (step.index_expression == nullptr) {
            record_expression_lowering_failure(
                session.failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto lowered_index = lowered_expression(
            *step.index_expression,
            "i64",
            IntegerSignedness::unsigned_integer,
            context,
            session,
            output
        );
        if (!lowered_index.has_value()) {
            return std::nullopt;
        }

        auto result = advance_aggregate_path_index_with_temporary(
            *cursor,
            lowered_index->value,
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error != AggregatePathError::none) {
            record_expression_lowering_failure(
                session.failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }
    }

    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    return emit_aggregate_path_cursor_load(
        *cursor,
        expected_llvm_type,
        expected_signedness,
        std::move(temporary_name),
        output
    );
}

auto lower_addressable_aggregate_path_read(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto path = collect_named_aggregate_path(expression);
    if (!path.has_value()) {
        return std::nullopt;
    }

    auto storage = named_aggregate_storage_for_name(path->base_expression->text, session.state);
    if (!storage.has_value()) {
        return std::nullopt;
    }

    if (!storage->source_type_name.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            expression.text
        );
        return std::nullopt;
    }

    return lower_aggregate_path_read_from_storage(
        expression,
        *path,
        std::move(storage->storage),
        *storage->source_type_name,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
}

auto lower_temporary_aggregate_path_read(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto path = collect_temporary_aggregate_path(expression);
    if (!path.has_value()) {
        return std::nullopt;
    }

    auto base_source_type =
        source_type_name_for_expression(*path->base_expression, context, session.state);
    if (!base_source_type.has_value()) {
        return std::nullopt;
    }

    auto base_llvm_type = llvm_type_for_source_type_name(*base_source_type, context.lowering);
    if (!base_llvm_type.has_value() || *base_llvm_type == "void") {
        return std::nullopt;
    }

    auto lowered_base = lowered_expression(
        *path->base_expression,
        *base_llvm_type,
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!lowered_base.has_value()) {
        return std::nullopt;
    }

    auto storage = spill_aggregate_value_to_temporary_storage(*lowered_base, session, output);
    if (!storage.has_value()) {
        return std::nullopt;
    }

    return lower_aggregate_path_read_from_storage(
        expression,
        *path,
        std::move(*storage),
        *base_source_type,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
}

auto emit_null_safe_empty_result(
    MaybeValueAbi const& result_abi,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> NullSafeIncoming {
    auto empty = emit_empty_maybe_value(result_abi, state.next_temporary_index, output);
    auto incoming_block = state.current_block;
    return NullSafeIncoming {
        .value = std::move(empty.value),
        .block = std::move(incoming_block),
    };
}

auto lower_null_safe_member_call_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::call || expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::null_safe_member_access ||
        expression.left->left == nullptr) {
        return std::nullopt;
    }

    auto resolved = resolve_member_call(expression, context, session.state);
    auto const receiver_name = expression.left->left != nullptr ? expression.left->left->text : std::string {};
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::unsupported_shape) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "member call receiver shape"
        );
        return std::nullopt;
    }
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::not_found) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unknown_member_call_receiver,
            receiver_name
        );
        return std::nullopt;
    }

    auto receiver_maybe_type_name = resolved.receiver.receiver_type_name;
    auto receiver_payload_type_name = maybe_payload_source_type_name(receiver_maybe_type_name);
    if (!receiver_payload_type_name.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe member call receiver ABI: " + resolved.receiver.receiver_type_name + "." +
                resolved.receiver.method_name
        );
        return std::nullopt;
    }

    auto method_lookup = find_lowered_method_signature(
        context.lowering,
        *receiver_payload_type_name,
        resolved.receiver.method_name
    );
    auto const target_name = *receiver_payload_type_name + "." + resolved.receiver.method_name;
    if (method_lookup.result == LoweredMethodLookupResult::not_found) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unknown_member_call_target,
            target_name
        );
        return std::nullopt;
    }
    if (method_lookup.result == LoweredMethodLookupResult::ambiguous) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::ambiguous_member_call_target,
            target_name
        );
        return std::nullopt;
    }
    if (method_lookup.method == nullptr ||
        !has_supported_function_signature_types(method_lookup.method->signature)) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "member call target is not lowerable: " + target_name
        );
        return std::nullopt;
    }

    auto const& method = method_lookup.method->signature;
    if (method.source_return_type_name.empty()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe member call return source type: " + target_name
        );
        return std::nullopt;
    }

    auto result_maybe_type_name = "Maybe<" + method.source_return_type_name + ">";
    auto result_abi = maybe_value_abi_for_source_type(result_maybe_type_name, context.lowering);
    if (!result_abi.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe member call result ABI: " + target_name
        );
        return std::nullopt;
    }
    if (result_abi->llvm_type != expected_llvm_type) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::type_mismatch,
            result_maybe_type_name + " has LLVM type " + result_abi->llvm_type +
                ", expected " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }

    auto receiver_abi = maybe_value_abi_for_source_type(receiver_maybe_type_name, context.lowering);
    if (!receiver_abi.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe member call receiver ABI: " + target_name
        );
        return std::nullopt;
    }

    auto lowered_receiver = lowered_expression(
        *expression.left->left,
        receiver_abi->llvm_type,
        IntegerSignedness::not_integer,
        context,
        session,
        output,
        receiver_maybe_type_name
    );
    if (!lowered_receiver.has_value()) {
        return std::nullopt;
    }

    auto merge_block = llvm_block_name(
        "nullsafe.call.merge",
        next_llvm_block_index(session.state.next_block_index)
    );
    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto some_block = llvm_block_name("nullsafe.call.some", block_index);
    auto empty_block = llvm_block_name("nullsafe.call.empty", block_index);

    auto tag_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << tag_name << " = extractvalue " << receiver_abi->llvm_type << " "
           << lowered_receiver->value << ", 0\n";
    output << "  br i1 " << tag_name << ", label %" << some_block
           << ", label %" << empty_block << "\n";

    session.state.current_block = empty_block;
    output << empty_block << ":\n";
    auto empty_incoming = emit_null_safe_empty_result(*result_abi, session.state, output);
    output << "  br label %" << merge_block << "\n";

    session.state.current_block = some_block;
    output << some_block << ":\n";
    auto payload_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << payload_name << " = extractvalue " << receiver_abi->llvm_type << " "
           << lowered_receiver->value << ", 1\n";
    auto receiver_payload = LoweredExpression {
        .type = receiver_abi->payload_llvm_type,
        .value = std::move(payload_name),
        .signedness = IntegerSignedness::not_integer,
    };

    auto expected_argument_count = method.parameter_types.size() - 1;
    if (expected_argument_count != expression.arguments.size()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::call_arity_mismatch,
            target_name + " expects " + std::to_string(expected_argument_count) +
                " arguments, got " + std::to_string(expression.arguments.size())
        );
        return std::nullopt;
    }

    auto arguments = lower_member_call_arguments(
        std::move(receiver_payload),
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
        if (session.failures.expression.reason == ExpressionLoweringFailureReason::none) {
            record_expression_lowering_failure(
                session.failures,
                ExpressionLoweringFailureReason::call_argument_failure,
                target_name
            );
        }
        return std::nullopt;
    }

    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    auto call_result = emit_value_call(std::move(temporary_name), method, *arguments, output);
    auto some_result = emit_some_maybe_value(
        *result_abi,
        call_result,
        session.state.next_temporary_index,
        output
    );
    if (!some_result.has_value()) {
        return std::nullopt;
    }
    auto some_incoming = NullSafeIncoming {
        .value = std::move(some_result->value),
        .block = session.state.current_block,
    };
    output << "  br label %" << merge_block << "\n";

    output << merge_block << ":\n";
    auto phi_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << phi_name << " = phi " << result_abi->llvm_type << " ["
           << empty_incoming.value << ", %" << empty_incoming.block << "], ["
           << some_incoming.value << ", %" << some_incoming.block << "]\n";
    session.state.current_block = merge_block;
    return LoweredExpression {
        .type = result_abi->llvm_type,
        .value = std::move(phi_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_null_safe_member_access_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::null_safe_member_access) {
        return std::nullopt;
    }

    auto plan_result = plan_null_safe_member_access(expression, context.lowering, session.state);
    if (!plan_result.plan.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            render_null_safe_plan_failure(plan_result.failure)
        );
        return std::nullopt;
    }

    auto const& plan = *plan_result.plan;
    auto result_abi = maybe_value_abi_for_source_type(plan.result_maybe_type_name, context.lowering);
    if (!result_abi.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe result ABI"
        );
        return std::nullopt;
    }
    if (result_abi->llvm_type != expected_llvm_type) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::type_mismatch,
            plan.result_maybe_type_name + " has LLVM type " + result_abi->llvm_type +
                ", expected " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }

    auto base_llvm_type = llvm_type_for_source_type_name(plan.base_maybe_type_name, context.lowering);
    if (!base_llvm_type.has_value()) {
        record_expression_lowering_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "null-safe base ABI"
        );
        return std::nullopt;
    }

    auto current_maybe = lowered_expression(
        *plan.base_expression,
        *base_llvm_type,
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!current_maybe.has_value()) {
        return std::nullopt;
    }

    auto current_maybe_type_name = plan.base_maybe_type_name;
    auto incoming = std::vector<NullSafeIncoming> {};
    auto merge_block = llvm_block_name(
        "nullsafe.merge",
        next_llvm_block_index(session.state.next_block_index)
    );
    for (auto index = std::size_t {0}; index < plan.segments.size(); ++index) {
        auto current_abi = maybe_value_abi_for_source_type(current_maybe_type_name, context.lowering);
        if (!current_abi.has_value() || current_maybe->type != current_abi->llvm_type) {
            record_expression_lowering_failure(
                session.failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "null-safe segment ABI"
            );
            return std::nullopt;
        }

        auto block_index = next_llvm_block_index(session.state.next_block_index);
        auto some_block = llvm_block_name("nullsafe.some", block_index);
        auto empty_block = llvm_block_name("nullsafe.empty", block_index);

        auto tag_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << tag_name << " = extractvalue " << current_abi->llvm_type << " "
               << current_maybe->value << ", 0\n";
        output << "  br i1 " << tag_name << ", label %" << some_block
               << ", label %" << empty_block << "\n";

        session.state.current_block = empty_block;
        output << empty_block << ":\n";
        incoming.push_back(emit_null_safe_empty_result(*result_abi, session.state, output));
        output << "  br label %" << merge_block << "\n";

        session.state.current_block = some_block;
        output << some_block << ":\n";
        auto payload_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << payload_name << " = extractvalue " << current_abi->llvm_type << " "
               << current_maybe->value << ", 1\n";
        auto payload = LoweredExpression {
            .type = current_abi->payload_llvm_type,
            .value = std::move(payload_name),
            .signedness = IntegerSignedness::not_integer,
        };

        auto const& segment = plan.segments[index];
        auto const* layout = context.lowering.records.contains(segment.receiver_type_name)
            ? &context.lowering.records.at(segment.receiver_type_name)
            : nullptr;
        auto const* field = layout != nullptr ? find_record_field(*layout, segment.field_name) : nullptr;
        auto field_llvm_type = llvm_type_for_source_type_name(segment.field_type_name, context.lowering);
        if (field == nullptr || !field_llvm_type.has_value()) {
            record_expression_lowering_failure(
                session.failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "null-safe field extraction"
            );
            return std::nullopt;
        }

        auto field_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << field_name << " = extractvalue " << payload.type << " "
               << payload.value << ", " << field->index << "\n";
        auto field_value = LoweredExpression {
            .type = *field_llvm_type,
            .value = std::move(field_name),
            .signedness = integer_signedness_for(syntax::TypeSyntax {.name = segment.field_type_name}),
        };

        auto field_maybe_payload = maybe_payload_source_type_name(segment.field_type_name);
        if (field_maybe_payload.has_value()) {
            current_maybe = std::move(field_value);
            current_maybe_type_name = segment.field_type_name;
        } else {
            auto segment_maybe_type_name = std::string {"Maybe<"} + segment.field_type_name + ">";
            auto segment_abi = maybe_value_abi_for_source_type(segment_maybe_type_name, context.lowering);
            if (!segment_abi.has_value()) {
                record_expression_lowering_failure(
                    session.failures,
                    ExpressionLoweringFailureReason::unsupported_expression,
                    "null-safe segment result ABI"
                );
                return std::nullopt;
            }
            current_maybe = emit_some_maybe_value(
                *segment_abi,
                field_value,
                session.state.next_temporary_index,
                output
            );
            if (!current_maybe.has_value()) {
                return std::nullopt;
            }
            current_maybe_type_name = std::move(segment_maybe_type_name);
        }

        if (index + 1 == plan.segments.size()) {
            incoming.push_back(NullSafeIncoming {
                .value = current_maybe->value,
                .block = session.state.current_block,
            });
            output << "  br label %" << merge_block << "\n";

            output << merge_block << ":\n";
            auto phi_name = next_llvm_temporary_name(session.state.next_temporary_index);
            output << "  " << phi_name << " = phi " << result_abi->llvm_type << " ";
            for (auto incoming_index = std::size_t {0}; incoming_index < incoming.size(); ++incoming_index) {
                if (incoming_index > 0) {
                    output << ", ";
                }
                output << "[" << incoming[incoming_index].value << ", %"
                       << incoming[incoming_index].block << "]";
            }
            output << "\n";
            session.state.current_block = std::move(merge_block);
            return LoweredExpression {
                .type = result_abi->llvm_type,
                .value = std::move(phi_name),
                .signedness = IntegerSignedness::not_integer,
            };
        }
    }

    return std::nullopt;
}

auto lowered_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
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
    if (auto array_literal = lower_array_literal_expression(
            expression,
            expected_llvm_type,
            context,
            session,
            failures,
            output,
            expected_source_type_name
        )) {
        return array_literal;
    }
    if (expression.kind == syntax::ExpressionKind::string_literal && expected_llvm_type == "ptr") {
        auto const* constant = context.string_constants.find(expression.text);
        if (constant == nullptr) {
            record_expression_lowering_failure(
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

    if (expression.kind == syntax::ExpressionKind::name && expression.text == "Empty") {
        if (auto maybe_abi = maybe_value_abi_for_llvm_type(expected_llvm_type, context.lowering)) {
            return emit_empty_maybe_value(*maybe_abi, state.next_temporary_index, output);
        }
    }

    if (auto choice_constructor = lower_choice_constructor_expression(
            expression,
            expected_llvm_type,
            context,
            session,
            failures,
            output,
            expected_source_type_name
        )) {
        return choice_constructor;
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "Some") {
        auto maybe_abi = expected_source_type_name.has_value()
            ? maybe_value_abi_for_source_type(*expected_source_type_name, context.lowering)
            : maybe_value_abi_for_llvm_type(expected_llvm_type, context.lowering);
        if (maybe_abi.has_value() && maybe_abi->llvm_type != expected_llvm_type) {
            maybe_abi = std::nullopt;
        }
        if (maybe_abi.has_value() && expression.arguments.size() == 1) {
            auto payload_source_type_name = maybe_abi->payload_source_type_name.empty()
                ? std::optional<std::string_view> {}
                : std::optional<std::string_view> {maybe_abi->payload_source_type_name};
            auto payload = lowered_expression(
                expression.arguments.front(),
                maybe_abi->payload_llvm_type,
                signedness_for_expected_array_element(
                    expression.arguments.front(),
                    maybe_abi->payload_llvm_type,
                    context,
                    state
                ),
                context,
                session,
                output,
                payload_source_type_name
            );
            if (!payload.has_value()) {
                return std::nullopt;
            }
            return emit_some_maybe_value(*maybe_abi, *payload, state.next_temporary_index, output);
        }
    }

    if (expression.kind == syntax::ExpressionKind::name) {
        auto binding = state.immutable_bindings.find(expression.text);
        if (binding == state.immutable_bindings.end()) {
            auto mutable_binding = state.mutable_bindings.find(expression.text);
            if (mutable_binding == state.mutable_bindings.end()) {
                record_expression_lowering_failure(failures, ExpressionLoweringFailureReason::unknown_name, expression.text);
                return std::nullopt;
            }
            if (mutable_binding->second.type.type != expected_llvm_type) {
                record_expression_lowering_failure(
                    failures,
                    ExpressionLoweringFailureReason::type_mismatch,
                    expression.text + " has LLVM type " + mutable_binding->second.type.type +
                        ", expected " + std::string(expected_llvm_type)
                );
                return std::nullopt;
            }
            if (mutable_binding->second.type.signedness != expected_signedness) {
                record_expression_lowering_failure(
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
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::type_mismatch,
                expression.text + " has LLVM type " + binding->second.type +
                    ", expected " + std::string(expected_llvm_type)
            );
            return std::nullopt;
        }
        if (binding->second.signedness != expected_signedness) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::signedness_mismatch,
                expression.text
            );
            return std::nullopt;
        }
        return binding->second;
    }

    if (auto aggregate_path_read = lower_addressable_aggregate_path_read(
            expression,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        )) {
        return aggregate_path_read;
    }

    if (auto aggregate_path_read = lower_temporary_aggregate_path_read(
            expression,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        )) {
        return aggregate_path_read;
    }

    if (auto null_safe_access = lower_null_safe_member_access_expression(
            expression,
            expected_llvm_type,
            context,
            session,
            output
        )) {
        return null_safe_access;
    }

    if (expression.kind == syntax::ExpressionKind::member_access && expression.left != nullptr) {
        auto base_source_type = source_type_name_for_expression(*expression.left, context, state);
        if (!base_source_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto layout = context.lowering.records.find(*base_source_type);
        if (layout == context.lowering.records.end()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto const* field = find_record_field(layout->second, expression.text);
        if (field == nullptr || field->source_type_name.empty()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto base_llvm_type = llvm_type_for_source_type_name(*base_source_type, context.lowering);
        auto field_llvm_type = llvm_type_for_source_type_name(field->source_type_name, context.lowering);
        auto field_signedness = integer_signedness_for(syntax::TypeSyntax {
            .name = field->source_type_name,
        });
        if (!base_llvm_type.has_value() || !field_llvm_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto lowered_base = lowered_expression(
            *expression.left,
            *base_llvm_type,
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!lowered_base.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        output << "  " << temporary_name << " = extractvalue " << lowered_base->type << " "
               << lowered_base->value << ", " << field->index << "\n";
        return LoweredExpression {
            .type = *field_llvm_type,
            .value = std::move(temporary_name),
            .signedness = field_signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::index_access && expression.left != nullptr &&
        expression.arguments.size() == 1) {
        auto base_source_type = source_type_name_for_expression(*expression.left, context, state);
        if (!base_source_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto element_source_type = array_element_source_type_name(*base_source_type);
        auto is_view_index = false;
        if (!element_source_type.has_value()) {
            element_source_type = view_element_source_type_name(*base_source_type);
            is_view_index = element_source_type.has_value();
        }
        if (!element_source_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto base_llvm_type = llvm_type_for_source_type_name(*base_source_type, context.lowering);
        auto element_llvm_type = llvm_type_for_source_type_name(*element_source_type, context.lowering);
        auto element_signedness = integer_signedness_for(syntax::TypeSyntax {
            .name = *element_source_type,
        });
        if (!base_llvm_type.has_value() || !element_llvm_type.has_value()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        if (*element_llvm_type != expected_llvm_type || element_signedness != expected_signedness) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::type_mismatch,
                expression.text
            );
            return std::nullopt;
        }

        auto lowered_base = lowered_expression(
            *expression.left,
            *base_llvm_type,
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!lowered_base.has_value()) {
            return std::nullopt;
        }

        auto lowered_index = lowered_expression(
            expression.arguments.front(),
            "i64",
            IntegerSignedness::unsigned_integer,
            context,
            session,
            output
        );
        if (!lowered_index.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        if (is_view_index) {
            auto element_pointer_name = next_llvm_temporary_name(state.next_temporary_index);
            output << "  " << element_pointer_name << " = getelementptr " << *element_llvm_type
                   << ", ptr " << lowered_base->value << ", i64 " << lowered_index->value << "\n";
            output << "  " << temporary_name << " = load " << *element_llvm_type << ", ptr "
                   << element_pointer_name << "\n";
        } else if (is_decimal_integer_text(lowered_index->value)) {
            output << "  " << temporary_name << " = extractvalue " << lowered_base->type << " "
                   << lowered_base->value << ", " << lowered_index->value << "\n";
        } else {
            auto storage_name =
                spill_aggregate_value_to_temporary_storage(*lowered_base, session, output);
            if (!storage_name.has_value()) {
                return std::nullopt;
            }
            auto element_pointer_name = next_llvm_temporary_name(state.next_temporary_index);
            output << "  " << element_pointer_name << " = getelementptr " << lowered_base->type
                   << ", ptr " << *storage_name << ", i64 0, i64 " << lowered_index->value << "\n";
            output << "  " << temporary_name << " = load " << *element_llvm_type << ", ptr "
                   << element_pointer_name << "\n";
        }
        return LoweredExpression {
            .type = *element_llvm_type,
            .value = std::move(temporary_name),
            .signedness = element_signedness,
        };
    }

    if (expression.kind == syntax::ExpressionKind::cast && expression.left != nullptr) {
        auto cast_type = lowered_type_for_source_type_name(expression.text, context.lowering);
        if (!cast_type.has_value() || cast_type->type != expected_llvm_type) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_cast,
                expression.text
            );
            return std::nullopt;
        }
        if (expression.left->kind == syntax::ExpressionKind::array_literal) {
            return lowered_expression(
                *expression.left,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                output,
                expression.text
            );
        }
        syntax::TypeSyntax target_type {
            .name = expression.text,
        };
        if (auto integer = lowered_integer_literal(
                *expression.left,
                expected_llvm_type,
                integer_signedness_for(target_type)
            )) {
            return integer;
        }
        auto lowered_float = lowered_float_literal(*expression.left, expected_llvm_type);
        if (!lowered_float.has_value()) {
            record_expression_lowering_failure(
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

    if (expression.kind == syntax::ExpressionKind::unary && expression.text == "bit_not" &&
        expression.left != nullptr && is_integer_llvm_type_impl(expected_llvm_type) &&
        expected_signedness != IntegerSignedness::not_integer) {
        auto operand = lowered_expression(
            *expression.left,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            output
        );
        if (!operand.has_value()) {
            return std::nullopt;
        }

        auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
        output << "  " << temporary_name << " = xor " << operand->type << " " << operand->value
               << ", -1\n";
        return LoweredExpression {
            .type = operand->type,
            .value = std::move(temporary_name),
            .signedness = operand->signedness,
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
            std::optional<std::string_view> expected_source_type_name;
        };
        auto arm_context = ArmContext {
            .then_expression = *expression.right,
            .else_expression = *expression.alternate,
            .expected_llvm_type = expected_llvm_type,
            .expected_signedness = expected_signedness,
            .context = context,
            .session = session,
            .output = output,
            .expected_source_type_name = expected_source_type_name,
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
                        arm.output,
                        arm.expected_source_type_name
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
                        arm.output,
                        arm.expected_source_type_name
                    );
                },
            }
        );
        if (result.failure == ConditionalEmissionFailure::branch_mismatch) {
            record_expression_lowering_failure(
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

            auto boolean_left_type = inferred_expression_type(*expression.left, context, state);
            auto boolean_right_type = inferred_expression_type(*expression.right, context, state);
            auto const has_boolean_operands =
                boolean_left_type.has_value() &&
                boolean_right_type.has_value() &&
                boolean_left_type->type == "i1" &&
                boolean_right_type->type == "i1" &&
                boolean_left_type->signedness == IntegerSignedness::not_integer &&
                boolean_right_type->signedness == IntegerSignedness::not_integer;
            if (has_boolean_operands) {
                auto boolean_predicate = llvm_boolean_comparison_predicate_for(expression.text);
                if (!boolean_predicate.has_value()) {
                    record_expression_lowering_failure(
                        failures,
                        ExpressionLoweringFailureReason::unsupported_operator,
                        expression.text
                    );
                    return std::nullopt;
                }
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
                output << "  " << temporary_name << " = icmp " << *boolean_predicate << " i1 ";
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
                record_expression_lowering_failure(
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
            record_expression_lowering_failure(
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
        return lower_null_safe_member_call_expression(expression, expected_llvm_type, context, session, output);
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "Pointer") {
        return lower_pointer_constructor_expression(
            expression,
            expected_llvm_type,
            context,
            session,
            failures,
            output
        );
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name &&
        is_low_level_intrinsic_name(expression.left->text)) {
        auto const& intrinsic_name = expression.left->text;
        if (intrinsic_name == "address_of") {
            return lower_address_of_intrinsic(
                expression,
                expected_llvm_type,
                context,
                failures,
                session,
                output
            );
        }
        if (intrinsic_name == "raw_offset") {
            return lower_raw_offset_intrinsic(
                expression,
                expected_llvm_type,
                context,
                session,
                failures,
                output
            );
        }
        if (intrinsic_name == "raw_read" || intrinsic_name == "volatile_read") {
            return lower_read_intrinsic(
                expression,
                expected_llvm_type,
                expected_signedness,
                intrinsic_name == "volatile_read",
                context,
                session,
                failures,
                output
            );
        }
        if (intrinsic_name == "raw_write" || intrinsic_name == "volatile_write") {
            return lower_write_intrinsic(
                expression,
                expected_llvm_type,
                intrinsic_name == "volatile_write",
                context,
                session,
                failures,
                output
            );
        }
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::member_access) {
        if (expression.left->text == "join" &&
            expression.left->left != nullptr &&
            expression.left->left->kind == syntax::ExpressionKind::name) {
            auto thread_binding = state.thread_bindings.find(expression.left->left->text);
            if (thread_binding != state.thread_bindings.end()) {
                return emit_thread_join_result(
                    thread_binding->second,
                    expected_llvm_type,
                    state,
                    failures,
                    output
                );
            }
        }

        auto resolved = resolve_member_call(expression, context, state);
        auto const receiver_name = expression.left->left != nullptr ? expression.left->left->text : std::string {};
        auto const target_name = resolved.receiver.receiver_type_name + "." + resolved.receiver.method_name;
        if (resolved.receiver.result == MemberCallReceiverInferenceResult::unsupported_shape) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "member call receiver shape"
            );
            return std::nullopt;
        }
        if (resolved.receiver.result == MemberCallReceiverInferenceResult::not_found) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unknown_member_call_receiver,
                receiver_name
            );
            return std::nullopt;
        }
        if (resolved.method.result == LoweredMethodLookupResult::not_found) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unknown_member_call_target,
                target_name
            );
            return std::nullopt;
        }
        if (resolved.method.result == LoweredMethodLookupResult::ambiguous) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::ambiguous_member_call_target,
                target_name
            );
            return std::nullopt;
        }
        if (resolved.method.method == nullptr ||
            !has_supported_function_signature_types(resolved.method.method->signature)) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "member call target is not lowerable: " + target_name
            );
            return std::nullopt;
        }

        auto const& method = resolved.method.method->signature;
        if (method.return_type != expected_llvm_type) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::call_return_type_mismatch,
                target_name + " returns " + method.return_type + ", expected " +
                    std::string(expected_llvm_type)
            );
            return std::nullopt;
        }

        auto const expected_argument_count = method.parameter_types.size() - 1;
        if (expected_argument_count != expression.arguments.size()) {
            record_expression_lowering_failure(
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
                record_expression_lowering_failure(
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

    if (expression.kind == syntax::ExpressionKind::unary &&
        expression.text == "await" &&
        expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        auto task_binding = state.task_bindings.find(expression.left->text);
        if (task_binding != state.task_bindings.end()) {
            return emit_task_await_result(
                task_binding->second,
                expected_llvm_type,
                state,
                failures,
                output
            );
        }
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        if (auto record_constructor = lower_record_constructor_expression(
                expression,
                expected_llvm_type,
                context,
                session,
                failures,
                output
            )) {
            return record_constructor;
        }

        auto function = context.lowering.functions.find(expression.left->text);
        if (function == context.lowering.functions.end()) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::unknown_function,
                expression.left->text
            );
            return std::nullopt;
        }
        if (function->second.return_type != expected_llvm_type) {
            record_expression_lowering_failure(
                failures,
                ExpressionLoweringFailureReason::call_return_type_mismatch,
                expression.left->text + " returns " + function->second.return_type +
                    ", expected " + std::string(expected_llvm_type)
            );
            return std::nullopt;
        }
        if (function->second.parameter_types.size() != expression.arguments.size()) {
            record_expression_lowering_failure(
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
                record_expression_lowering_failure(
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

    record_expression_lowering_failure(
        failures,
        is_concurrency_expression(expression)
            ? ExpressionLoweringFailureReason::unsupported_concurrency_expression
            : ExpressionLoweringFailureReason::unsupported_expression,
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
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto& failures = session.failures;
    reset_expression_lowering_failure(failures);
    return lowered_expression(
        expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output,
        expected_source_type_name
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
