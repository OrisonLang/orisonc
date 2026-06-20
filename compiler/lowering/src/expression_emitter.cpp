#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/conditional_emitter.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/function_signature.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <algorithm>
#include <cctype>
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
    std::ostringstream& output
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

        auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << temporary_name << " = ptrtoint ptr " << pointer->value << " to i64\n";
        return LoweredExpression {
            .type = "i64",
            .value = std::move(temporary_name),
            .signedness = IntegerSignedness::not_integer,
        };
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
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::array_literal) {
        return std::nullopt;
    }

    auto array_type = parse_llvm_array_type(expected_llvm_type);
    if (!array_type.has_value()) {
        record_failure(
            failures,
            ExpressionLoweringFailureReason::type_mismatch,
            "array literal expected LLVM array type " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }
    if (expression.arguments.size() != array_type->length) {
        record_failure(
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
            output
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
        record_failure(
            failures,
            ExpressionLoweringFailureReason::type_mismatch,
            expression.left->text + " has LLVM type " + layout->second.llvm_type_name +
                ", expected " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }
    if (layout->second.fields.size() != expression.arguments.size()) {
        record_failure(
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
            record_failure(
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
            output
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

    auto current_pointer_value = std::string {};
    auto current_source_type_name = std::string {};
    if (auto record_name = pointer_pointee_source_type_name(*base_source_type)) {
        auto base_pointer = lower_pointer_operand(base_expression, context, session, output);
        if (!base_pointer.has_value()) {
            return std::nullopt;
        }
        current_pointer_value = std::move(base_pointer->value);
        current_source_type_name = *record_name;
    } else if (base_expression.kind == syntax::ExpressionKind::name) {
        auto binding = session.state.mutable_bindings.find(base_expression.text);
        if (binding == session.state.mutable_bindings.end()) {
            return std::nullopt;
        }
        current_pointer_value = binding->second.storage;
        current_source_type_name = *base_source_type;
    } else {
        return std::nullopt;
    }

    auto cursor = initialize_aggregate_path_cursor(
        std::move(current_pointer_value),
        std::move(current_source_type_name),
        context.lowering
    );
    if (!cursor.has_value()) {
        record_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of field source record layout"
        );
        return std::nullopt;
    }

    for (auto const& step : path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto field_pointer_name = next_llvm_temporary_name(session.state.next_temporary_index);
            auto result = advance_aggregate_path_member(
                *cursor,
                step.field_name,
                context.lowering,
                std::move(field_pointer_name),
                output
            );
            if (result.error != AggregatePathError::none) {
                record_failure(
                    failures,
                    ExpressionLoweringFailureReason::unsupported_expression,
                    "address_of field layout"
                );
                return std::nullopt;
            }
            continue;
        }

        if (step.index_expression == nullptr) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "address_of indexed field layout"
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

        auto element_pointer_name = next_llvm_temporary_name(session.state.next_temporary_index);
        auto result = advance_aggregate_path_index(
            *cursor,
            lowered_index->value,
            context.lowering,
            std::move(element_pointer_name),
            output
        );
        if (result.error != AggregatePathError::none) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                result.error == AggregatePathError::unsupported_element_source_type
                    ? "address_of indexed source type"
                    : "address_of indexed field layout"
            );
            return std::nullopt;
        }
    }

    auto address_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << address_name << " = ptrtoint ptr " << cursor->pointer << " to i64\n";
    return LoweredExpression {
        .type = "i64",
        .value = std::move(address_name),
        .signedness = IntegerSignedness::not_integer,
    };
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
        record_failure(
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
        record_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of currently only lowers mutable local names and aggregate fields"
        );
        return std::nullopt;
    }

    auto binding = session.state.mutable_bindings.find(operand.text);
    if (binding == session.state.mutable_bindings.end()) {
        record_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of currently only lowers mutable local names and aggregate fields"
        );
        return std::nullopt;
    }

    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = ptrtoint ptr " << binding->second.storage << " to i64\n";
    return LoweredExpression {
        .type = "i64",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
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
        record_failure(
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

        auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << temporary_name << " = add i64 " << address->value << ", " << offset->value
               << "\n";
        return LoweredExpression {
            .type = "i64",
            .value = std::move(temporary_name),
            .signedness = IntegerSignedness::not_integer,
        };
    }

    if (expected_llvm_type != "ptr") {
        record_failure(
            failures,
            ExpressionLoweringFailureReason::type_mismatch,
            "raw_offset expected ptr or i64 result"
        );
        return std::nullopt;
    }

    auto pointee_type =
        pointee_lowered_type_for_pointer_expression(expression.arguments.front(), context, session.state);
    if (!pointee_type.has_value()) {
        record_failure(
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

    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = getelementptr " << pointee_type->type << ", ptr "
           << pointer->value << ", i64 " << offset->value << "\n";
    return LoweredExpression {
        .type = "ptr",
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
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
        record_failure(
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

    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = load ";
    if (is_volatile) {
        output << "volatile ";
    }
    output << expected_llvm_type << ", ptr " << pointer->value << "\n";
    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = std::move(temporary_name),
        .signedness = expected_signedness,
    };
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
        record_failure(
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
        record_failure(
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

    output << "  store ";
    if (is_volatile) {
        output << "volatile ";
    }
    output << value_type->type << " " << value->value << ", ptr " << pointer->value << "\n";
    return LoweredExpression {
        .type = "void",
        .value = "",
        .signedness = IntegerSignedness::not_integer,
    };
}

auto lower_addressable_aggregate_path_read(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (expression.kind != syntax::ExpressionKind::member_access &&
        expression.kind != syntax::ExpressionKind::index_access) {
        return std::nullopt;
    }

    auto path = collect_aggregate_path(expression);
    if (path.steps.empty() || path.base_expression == nullptr ||
        path.base_expression->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto storage = std::string {};
    if (auto binding = session.state.mutable_bindings.find(path.base_expression->text);
        binding != session.state.mutable_bindings.end()) {
        storage = binding->second.storage;
    } else if (auto binding = session.state.addressable_bindings.find(path.base_expression->text);
               binding != session.state.addressable_bindings.end()) {
        storage = binding->second.storage;
    } else {
        return std::nullopt;
    }

    auto inferred = inferred_expression_type(expression, context, session.state);
    if (!inferred.has_value()) {
        record_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            expression.text
        );
        return std::nullopt;
    }
    if (inferred->type != expected_llvm_type) {
        record_failure(
            session.failures,
            ExpressionLoweringFailureReason::type_mismatch,
            expression.text
        );
        return std::nullopt;
    }
    if (inferred->signedness != expected_signedness) {
        record_failure(
            session.failures,
            ExpressionLoweringFailureReason::signedness_mismatch,
            expression.text
        );
        return std::nullopt;
    }

    auto base_source_type =
        source_type_name_for_expression(*path.base_expression, context, session.state);
    if (!base_source_type.has_value()) {
        record_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            expression.text
        );
        return std::nullopt;
    }

    auto cursor = initialize_aggregate_path_cursor(
        std::move(storage),
        *base_source_type,
        context.lowering
    );
    if (!cursor.has_value()) {
        record_failure(
            session.failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            expression.text
        );
        return std::nullopt;
    }

    for (auto const& step : path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto field_pointer_name = next_llvm_temporary_name(session.state.next_temporary_index);
            auto result = advance_aggregate_path_member(
                *cursor,
                step.field_name,
                context.lowering,
                std::move(field_pointer_name),
                output
            );
            if (result.error != AggregatePathError::none) {
                record_failure(
                    session.failures,
                    ExpressionLoweringFailureReason::unsupported_expression,
                    expression.text
                );
                return std::nullopt;
            }
            continue;
        }

        if (step.index_expression == nullptr) {
            record_failure(
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

        auto element_pointer_name = next_llvm_temporary_name(session.state.next_temporary_index);
        auto result = advance_aggregate_path_index(
            *cursor,
            lowered_index->value,
            context.lowering,
            std::move(element_pointer_name),
            output
        );
        if (result.error != AggregatePathError::none) {
            record_failure(
                session.failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }
    }

    auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << temporary_name << " = load " << expected_llvm_type << ", ptr "
           << cursor->pointer << "\n";
    return LoweredExpression {
        .type = std::string(expected_llvm_type),
        .value = std::move(temporary_name),
        .signedness = expected_signedness,
    };
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
    if (auto array_literal = lower_array_literal_expression(
            expression,
            expected_llvm_type,
            context,
            session,
            failures,
            output
        )) {
        return array_literal;
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

    if (expression.kind == syntax::ExpressionKind::member_access && expression.left != nullptr) {
        auto base_source_type = source_type_name_for_expression(*expression.left, context, state);
        if (!base_source_type.has_value()) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto layout = context.lowering.records.find(*base_source_type);
        if (layout == context.lowering.records.end()) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto const* field = find_record_field(layout->second, expression.text);
        if (field == nullptr || field->source_type_name.empty()) {
            record_failure(
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
            record_failure(
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
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        auto element_source_type = array_element_source_type_name(*base_source_type);
        if (!element_source_type.has_value()) {
            record_failure(
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
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                expression.text
            );
            return std::nullopt;
        }

        if (*element_llvm_type != expected_llvm_type || element_signedness != expected_signedness) {
            record_failure(
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
        if (is_decimal_integer_text(lowered_index->value)) {
            output << "  " << temporary_name << " = extractvalue " << lowered_base->type << " "
                   << lowered_base->value << ", " << lowered_index->value << "\n";
        } else {
            auto storage_name = next_llvm_temporary_name(state.next_temporary_index);
            auto element_pointer_name = next_llvm_temporary_name(state.next_temporary_index);
            output << "  " << storage_name << " = alloca " << lowered_base->type << "\n";
            output << "  store " << lowered_base->type << " " << lowered_base->value << ", ptr "
                   << storage_name << "\n";
            output << "  " << element_pointer_name << " = getelementptr " << lowered_base->type
                   << ", ptr " << storage_name << ", i64 0, i64 " << lowered_index->value << "\n";
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
