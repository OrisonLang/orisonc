#include "orison/lowering/call_emitter.hpp"

#include "orison/lowering/c_abi_adapter.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <cstddef>
#include <span>
#include <sstream>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

auto append_call_signature(
    std::ostringstream& output,
    LoweredFunctionSignature const& function
) -> void {
    output << function.return_type;
    if (function.adapter == CAbiAdapterKind::variadic) {
        output << " (";
        for (auto index = std::size_t {0}; index < function.fixed_abi_parameter_count; ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << function.parameter_types[index];
        }
        if (function.fixed_abi_parameter_count > 0) {
            output << ", ";
        }
        output << "...)";
    }
}

auto append_call_arguments(
    std::ostringstream& output,
    std::vector<LoweredExpression> const& arguments
) -> void {
    for (auto index = std::size_t {0}; index < arguments.size(); ++index) {
        if (index > 0) {
            output << ", ";
        }
        output << arguments[index].type << " " << arguments[index].value;
    }
}

auto is_owned_dynamic_array_source_type(std::string_view source_type_name) -> bool {
    auto sequence = dynamic_sequence_source_type(source_type_name);
    return sequence.has_value() && sequence->kind == DynamicSequenceKind::dynamic_array &&
        sequence->owns_storage;
}

auto has_local_dynamic_array_cleanup_plan(
    FunctionLoweringSession const& session,
    std::string_view owner_name
) -> bool {
    for (auto const& plan : session.state.dynamic_array_local_cleanup_plans) {
        if (plan.owner_name == owner_name) {
            return true;
        }
    }
    return false;
}

auto has_function_parameter_name(
    FunctionLoweringSession const& session,
    std::string_view name
) -> bool {
    for (auto const& parameter_name : session.state.parameter_names) {
        if (parameter_name == name) {
            return true;
        }
    }
    return false;
}

auto has_dynamic_array_cleanup_owner(
    FunctionLoweringSession const& session,
    std::string_view owner_name
) -> bool {
    return has_local_dynamic_array_cleanup_plan(session, owner_name) ||
        has_function_parameter_name(session, owner_name);
}

auto consumed_owned_dynamic_array_argument_name(
    syntax::ExpressionSyntax const& argument,
    std::optional<std::string_view> expected_source_type,
    FunctionLoweringSession const& session
) -> std::optional<std::string> {
    if (!expected_source_type.has_value() ||
        !is_owned_dynamic_array_source_type(*expected_source_type) ||
        argument.kind != syntax::ExpressionKind::name ||
        !has_dynamic_array_cleanup_owner(session, argument.text)) {
        return std::nullopt;
    }

    auto actual_source_type = session.state.source_type_names.find(argument.text);
    if (actual_source_type == session.state.source_type_names.end() ||
        actual_source_type->second != *expected_source_type ||
        !is_owned_dynamic_array_source_type(actual_source_type->second)) {
        return std::nullopt;
    }

    return argument.text;
}

auto lower_call_arguments_impl(
    syntax::ExpressionSyntax const* receiver_expression,
    LoweredExpression const* lowered_receiver_expression,
    std::span<syntax::ExpressionSyntax const> arguments,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<std::vector<LoweredExpression>> {
    auto const has_receiver = receiver_expression != nullptr || lowered_receiver_expression != nullptr;
    auto const expected_argument_count = arguments.size() + (has_receiver ? 1 : 0);
    if (function.parameter_types.size() != expected_argument_count) {
        return std::nullopt;
    }

    auto lowered_arguments = std::vector<LoweredExpression> {};
    lowered_arguments.reserve(expected_argument_count);
    auto expected_source_type_for_parameter = [&](std::size_t index) -> std::optional<std::string_view> {
        if (index >= function.parameter_source_type_names.size() ||
            function.parameter_source_type_names[index].empty()) {
            return std::nullopt;
        }
        return function.parameter_source_type_names[index];
    };

    auto parameter_index = std::size_t {0};
    if (receiver_expression != nullptr) {
        auto lowered_receiver = lower_expression(
            *receiver_expression,
            function.parameter_types.front(),
            function.parameter_signedness.front(),
            context,
            session,
            output,
            expected_source_type_for_parameter(0)
        );
        if (!lowered_receiver.has_value()) {
            return std::nullopt;
        }
        lowered_arguments.push_back(std::move(*lowered_receiver));
        parameter_index = 1;
    } else if (lowered_receiver_expression != nullptr) {
        if (function.parameter_types.empty() ||
            lowered_receiver_expression->type != function.parameter_types.front()) {
            return std::nullopt;
        }
        lowered_arguments.push_back(*lowered_receiver_expression);
        parameter_index = 1;
    }

    for (auto index = std::size_t {0}; index < arguments.size(); ++index) {
        auto const actual_parameter_index = index + parameter_index;
        auto const expected_source_type = expected_source_type_for_parameter(actual_parameter_index);
        auto argument = lower_expression(
            arguments[index],
            function.parameter_types[actual_parameter_index],
            function.parameter_signedness[actual_parameter_index],
            context,
            session,
            output,
            expected_source_type
        );
        if (!argument.has_value()) {
            return std::nullopt;
        }

        auto promotion = actual_parameter_index >= function.fixed_abi_parameter_count
            ? c_abi_promotion_for(argument->type)
            : CAbiPromotion::none;
        if (function.adapter == CAbiAdapterKind::variadic &&
            promotion == CAbiPromotion::integer_to_i32) {
            auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
            auto instruction =
                argument->signedness == IntegerSignedness::signed_integer ? "sext" : "zext";
            output << "  " << temporary_name << " = " << instruction << " " << argument->type
                   << " " << argument->value << " to i32\n";
            argument = LoweredExpression {
                .type = "i32",
                .value = std::move(temporary_name),
                .signedness = argument->signedness,
            };
        }
        if (function.adapter == CAbiAdapterKind::variadic &&
            promotion == CAbiPromotion::float_to_double) {
            auto temporary_name = next_llvm_temporary_name(session.state.next_temporary_index);
            output << "  " << temporary_name << " = fpext float " << argument->value
                   << " to double\n";
            argument = LoweredExpression {
                .type = "double",
                .value = std::move(temporary_name),
                .signedness = IntegerSignedness::not_integer,
            };
        }
        if (auto consumed_name = consumed_owned_dynamic_array_argument_name(
                arguments[index],
                expected_source_type,
                session
            )) {
            session.state.consumed_owned_dynamic_array_bindings.insert(std::move(*consumed_name));
        }
        lowered_arguments.push_back(std::move(*argument));
    }

    return lowered_arguments;
}

}  // namespace

auto lower_call_arguments(
    syntax::ExpressionSyntax const& expression,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<std::vector<LoweredExpression>> {
    return lower_call_arguments_impl(
        nullptr,
        nullptr,
        std::span<syntax::ExpressionSyntax const>(expression.arguments.data(), expression.arguments.size()),
        function,
        context,
        session,
        output
    );
}

auto lower_member_call_arguments(
    syntax::ExpressionSyntax const& receiver_expression,
    std::span<syntax::ExpressionSyntax const> arguments,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<std::vector<LoweredExpression>> {
    return lower_call_arguments_impl(
        &receiver_expression,
        nullptr,
        arguments,
        function,
        context,
        session,
        output
    );
}

auto lower_member_call_arguments(
    LoweredExpression receiver,
    std::span<syntax::ExpressionSyntax const> arguments,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<std::vector<LoweredExpression>> {
    return lower_call_arguments_impl(
        nullptr,
        &receiver,
        arguments,
        function,
        context,
        session,
        output
    );
}

auto emit_value_call(
    std::string temporary_name,
    LoweredFunctionSignature const& function,
    std::vector<LoweredExpression> const& arguments,
    std::ostringstream& output
) -> LoweredExpression {
    auto const temporary_value = temporary_name;
    output << "  " << temporary_name << " = call ";
    append_call_signature(output, function);
    output << " @" << function.symbol_name << "(";
    append_call_arguments(output, arguments);
    output << ")\n";
    return LoweredExpression {
        .type = function.return_type,
        .value = temporary_value,
        .signedness = function.return_signedness,
    };
}

auto emit_void_call(
    LoweredFunctionSignature const& function,
    std::vector<LoweredExpression> const& arguments,
    std::ostringstream& output
) -> void {
    output << "  call ";
    append_call_signature(output, function);
    output << " @" << function.symbol_name << "(";
    append_call_arguments(output, arguments);
    output << ")\n";
}

}  // namespace orison::lowering
