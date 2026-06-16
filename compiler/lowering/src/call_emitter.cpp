#include "orison/lowering/call_emitter.hpp"

#include "orison/lowering/c_abi_adapter.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_names.hpp"

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

auto lower_call_arguments_impl(
    syntax::ExpressionSyntax const* receiver_expression,
    std::span<syntax::ExpressionSyntax const> arguments,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> std::optional<std::vector<LoweredExpression>> {
    auto const expected_argument_count = arguments.size() + (receiver_expression == nullptr ? 0 : 1);
    if (function.parameter_types.size() != expected_argument_count) {
        return std::nullopt;
    }

    auto lowered_arguments = std::vector<LoweredExpression> {};
    lowered_arguments.reserve(expected_argument_count);

    auto parameter_index = std::size_t {0};
    if (receiver_expression != nullptr) {
        auto lowered_receiver = lower_expression(
            *receiver_expression,
            function.parameter_types.front(),
            function.parameter_signedness.front(),
            context,
            session,
            output
        );
        if (!lowered_receiver.has_value()) {
            return std::nullopt;
        }
        lowered_arguments.push_back(std::move(*lowered_receiver));
        parameter_index = 1;
    }

    for (auto index = std::size_t {0}; index < arguments.size(); ++index) {
        auto argument = lower_expression(
            arguments[index],
            function.parameter_types[index + parameter_index],
            function.parameter_signedness[index + parameter_index],
            context,
            session,
            output
        );
        if (!argument.has_value()) {
            return std::nullopt;
        }

        auto const actual_parameter_index = index + parameter_index;
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
