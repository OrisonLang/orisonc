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

#include <algorithm>
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

auto lowered_expression(
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

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
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto source_type = state.source_type_names.find(expression.text);
        if (source_type != state.source_type_names.end()) {
            return source_type->second;
        }
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name && expression.left->text == "raw_offset" &&
        !expression.arguments.empty()) {
        return source_type_name_for_expression(expression.arguments.front(), state);
    }

    return std::nullopt;
}

auto pointee_lowered_type_for_pointer_expression(
    syntax::ExpressionSyntax const& expression,
    FunctionLoweringState const& state
) -> std::optional<LoweredType> {
    auto source_type = source_type_name_for_expression(expression, state);
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
                pointee_lowered_type_for_pointer_expression(expression.arguments.front(), state);
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

    auto source_type = source_type_name_for_expression(expression, session.state);
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

auto find_record_field(
    LoweredRecordLayout const& layout,
    std::string_view field_name
) -> LoweredRecordField const* {
    for (auto const& field : layout.fields) {
        if (field.name == field_name) {
            return &field;
        }
    }
    return nullptr;
}

auto is_array_llvm_type(std::string_view type) -> bool {
    return type.starts_with("[");
}

struct AddressPathStep {
    enum class Kind {
        member,
        index,
    };

    Kind kind = Kind::member;
    std::string field_name;
    syntax::ExpressionSyntax const* index_expression = nullptr;
};

auto split_top_level_generic_arguments(std::string_view text) -> std::vector<std::string> {
    auto arguments = std::vector<std::string> {};
    auto depth = std::size_t {0};
    auto start = std::size_t {0};
    for (auto index = std::size_t {0}; index < text.size(); ++index) {
        auto const character = text[index];
        if (character == '<') {
            ++depth;
            continue;
        }
        if (character == '>') {
            if (depth > 0) {
                --depth;
            }
            continue;
        }
        if (character == ',' && depth == 0) {
            auto argument = std::string {text.substr(start, index - start)};
            if (!argument.empty() && argument.front() == ' ') {
                argument.erase(argument.begin());
            }
            if (!argument.empty() && argument.back() == ' ') {
                argument.pop_back();
            }
            arguments.push_back(std::move(argument));
            start = index + 1;
        }
    }

    if (start < text.size()) {
        auto argument = std::string {text.substr(start)};
        if (!argument.empty() && argument.front() == ' ') {
            argument.erase(argument.begin());
        }
        if (!argument.empty() && argument.back() == ' ') {
            argument.pop_back();
        }
        arguments.push_back(std::move(argument));
    }
    return arguments;
}

auto array_element_source_type_name(std::string_view type_name) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"Array<"};
    if (!type_name.starts_with(prefix) || !type_name.ends_with(">") ||
        type_name.size() <= prefix.size() + 1) {
        return std::nullopt;
    }

    auto arguments = split_top_level_generic_arguments(
        type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1)
    );
    if (arguments.size() != 2 || arguments[0].empty()) {
        return std::nullopt;
    }
    return arguments[0];
}

auto llvm_type_for_source_type_name(
    std::string_view type_name,
    LoweringContext const& context
) -> std::optional<std::string> {
    if (auto lowered = lowered_type_for_source_type_name(type_name)) {
        return lowered->type;
    }

    if (auto record = context.records.find(std::string(type_name)); record != context.records.end()) {
        return record->second.llvm_type_name;
    }

    constexpr auto prefix = std::string_view {"Array<"};
    if (type_name.starts_with(prefix) && type_name.ends_with(">") &&
        type_name.size() > prefix.size() + 1) {
        auto arguments = split_top_level_generic_arguments(
            type_name.substr(prefix.size(), type_name.size() - prefix.size() - 1)
        );
        if (arguments.size() == 2 && !arguments[1].empty()) {
            auto element_type = llvm_type_for_source_type_name(arguments[0], context);
            if (element_type.has_value()) {
                return "[" + arguments[1] + " x " + *element_type + "]";
            }
        }
    }

    return std::nullopt;
}

auto lower_pointer_record_field_address(
    syntax::ExpressionSyntax const& operand,
    LoweringEmissionContext const& context,
    LoweringFailures& failures,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto steps = std::vector<AddressPathStep> {};
    auto const* base_expression = &operand;
    while (true) {
        if (base_expression->kind == syntax::ExpressionKind::member_access &&
            base_expression->left != nullptr) {
            steps.push_back(AddressPathStep {
                .kind = AddressPathStep::Kind::member,
                .field_name = base_expression->text,
            });
            base_expression = base_expression->left.get();
            continue;
        }
        if (base_expression->kind == syntax::ExpressionKind::index_access &&
            base_expression->left != nullptr && base_expression->arguments.size() == 1) {
            steps.push_back(AddressPathStep {
                .kind = AddressPathStep::Kind::index,
                .index_expression = &base_expression->arguments.front(),
            });
            base_expression = base_expression->left.get();
            continue;
        }
        break;
    }
    std::reverse(steps.begin(), steps.end());
    if (steps.empty() || steps.front().kind != AddressPathStep::Kind::member) {
        return std::nullopt;
    }

    auto base_source_type = source_type_name_for_expression(*base_expression, session.state);
    if (!base_source_type.has_value()) {
        return std::nullopt;
    }

    auto record_name = pointer_pointee_source_type_name(*base_source_type);
    if (!record_name.has_value()) {
        return std::nullopt;
    }

    auto layout = context.lowering.records.find(*record_name);
    if (layout == context.lowering.records.end()) {
        record_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of field source record layout"
        );
        return std::nullopt;
    }

    auto base_pointer = lower_pointer_operand(*base_expression, context, session, output);
    if (!base_pointer.has_value()) {
        return std::nullopt;
    }

    auto current_pointer_value = base_pointer->value;
    auto current_layout = &layout->second;
    auto current_source_type_name = std::string {*record_name};
    auto current_llvm_type_name = current_layout->llvm_type_name;
    auto expect_record_layout = true;

    for (auto index = std::size_t {0}; index < steps.size(); ++index) {
        auto const& step = steps[index];
        if (step.kind == AddressPathStep::Kind::member) {
            if (!expect_record_layout || current_layout == nullptr) {
                record_failure(
                    failures,
                    ExpressionLoweringFailureReason::unsupported_expression,
                    "address_of field layout"
                );
                return std::nullopt;
            }

            auto const* field = find_record_field(*current_layout, step.field_name);
            if (field == nullptr || field->llvm_type.empty() || field->llvm_type == "void") {
                record_failure(
                    failures,
                    ExpressionLoweringFailureReason::unsupported_expression,
                    "address_of field layout"
                );
                return std::nullopt;
            }

            auto field_pointer_name = next_llvm_temporary_name(session.state.next_temporary_index);
            output << "  " << field_pointer_name << " = getelementptr " << current_llvm_type_name
                   << ", ptr " << current_pointer_value << ", i32 0, i32 " << field->index << "\n";
            current_pointer_value = std::move(field_pointer_name);
            current_source_type_name = field->source_type_name;
            current_llvm_type_name = field->llvm_type;

            auto nested_layout = context.lowering.records.find(current_source_type_name);
            if (nested_layout != context.lowering.records.end()) {
                current_layout = &nested_layout->second;
                current_llvm_type_name = current_layout->llvm_type_name;
                expect_record_layout = true;
            } else {
                current_layout = nullptr;
                expect_record_layout = false;
            }
            continue;
        }

        if (step.index_expression == nullptr || !is_array_llvm_type(current_llvm_type_name)) {
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
        output << "  " << element_pointer_name << " = getelementptr " << current_llvm_type_name
               << ", ptr " << current_pointer_value << ", i64 0, i64 " << lowered_index->value
               << "\n";
        current_pointer_value = std::move(element_pointer_name);
        auto element_source_type = array_element_source_type_name(current_source_type_name);
        if (!element_source_type.has_value()) {
            record_failure(
                failures,
                ExpressionLoweringFailureReason::unsupported_expression,
                "address_of indexed source type"
            );
            return std::nullopt;
        }
        current_source_type_name = *element_source_type;

        auto nested_layout = context.lowering.records.find(current_source_type_name);
        if (nested_layout != context.lowering.records.end()) {
            current_layout = &nested_layout->second;
            current_llvm_type_name = current_layout->llvm_type_name;
            expect_record_layout = true;
            continue;
        }

        auto element_llvm_type =
            llvm_type_for_source_type_name(current_source_type_name, context.lowering);
        if (!element_llvm_type.has_value()) {
            current_layout = nullptr;
            current_llvm_type_name.clear();
            expect_record_layout = false;
            continue;
        }
        current_layout = nullptr;
        current_llvm_type_name = *element_llvm_type;
        expect_record_layout = false;
    }

    auto address_name = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << address_name << " = ptrtoint ptr " << current_pointer_value << " to i64\n";
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
            "address_of currently only lowers mutable local names and Pointer<Record> fields"
        );
        return std::nullopt;
    }

    auto binding = session.state.mutable_bindings.find(operand.text);
    if (binding == session.state.mutable_bindings.end()) {
        record_failure(
            failures,
            ExpressionLoweringFailureReason::unsupported_expression,
            "address_of currently only lowers mutable local names and Pointer<Record> fields"
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
        pointee_lowered_type_for_pointer_expression(expression.arguments.front(), session.state);
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
        pointee_lowered_type_for_pointer_expression(expression.arguments.front(), session.state);
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
