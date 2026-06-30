#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/c_abi_adapter.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <vector>

int main() {
    using orison::lowering::CAbiAdapterKind;
    using orison::lowering::IntegerSignedness;
    using orison::lowering::LoweredExpression;
    using orison::lowering::LoweredFunctionSignature;

    auto add = LoweredFunctionSignature {
        .return_type = "i32",
        .return_signedness = IntegerSignedness::unsigned_integer,
        .parameter_types = {"i32", "i32"},
        .parameter_signedness = {
            IntegerSignedness::unsigned_integer,
            IntegerSignedness::unsigned_integer,
        },
        .symbol_name = "add",
    };
    auto add_arguments = std::vector<LoweredExpression> {
        LoweredExpression {
            .type = "i32",
            .value = "40",
            .signedness = IntegerSignedness::unsigned_integer,
        },
        LoweredExpression {
            .type = "i32",
            .value = "2",
            .signedness = IntegerSignedness::unsigned_integer,
        },
    };

    auto output = std::ostringstream {};
    auto result = orison::lowering::emit_value_call("%tmp0", add, add_arguments, output);
    assert(output.str() == "  %tmp0 = call i32 @add(i32 40, i32 2)\n");
    assert(result.type == "i32");
    assert(result.value == "%tmp0");
    assert(result.signedness == IntegerSignedness::unsigned_integer);

    auto observe = LoweredFunctionSignature {
        .return_type = "void",
        .parameter_types = {"i32"},
        .parameter_signedness = {IntegerSignedness::signed_integer},
        .symbol_name = "observe",
    };
    auto observe_arguments = std::vector<LoweredExpression> {
        LoweredExpression {
            .type = "i32",
            .value = "%value",
            .signedness = IntegerSignedness::signed_integer,
        },
    };
    auto void_output = std::ostringstream {};
    orison::lowering::emit_void_call(observe, observe_arguments, void_output);
    assert(void_output.str() == "  call void @observe(i32 %value)\n");

    auto printf = LoweredFunctionSignature {
        .return_type = "i32",
        .return_signedness = IntegerSignedness::signed_integer,
        .parameter_types = {"ptr", "i32"},
        .parameter_signedness = {
            IntegerSignedness::not_integer,
            IntegerSignedness::signed_integer,
        },
        .symbol_name = "printf",
        .adapter = CAbiAdapterKind::variadic,
        .fixed_abi_parameter_count = 1,
    };
    auto printf_arguments = std::vector<LoweredExpression> {
        LoweredExpression {
            .type = "ptr",
            .value = "@.str.0",
            .signedness = IntegerSignedness::not_integer,
        },
        LoweredExpression {
            .type = "i32",
            .value = "7",
            .signedness = IntegerSignedness::signed_integer,
        },
    };
    auto printf_output = std::ostringstream {};
    orison::lowering::emit_value_call("%tmp1", printf, printf_arguments, printf_output);
    assert(printf_output.str() == "  %tmp1 = call i32 (ptr, ...) @printf(ptr @.str.0, i32 7)\n");

    auto scale = LoweredFunctionSignature {
        .return_type = "i32",
        .return_signedness = IntegerSignedness::unsigned_integer,
        .parameter_types = {"i32", "i32"},
        .parameter_signedness = {
            IntegerSignedness::unsigned_integer,
            IntegerSignedness::unsigned_integer,
        },
        .symbol_name = "method.UInt32.scale",
    };
    auto receiver = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::name,
        .text = "value",
    };
    auto scale_arguments = std::vector<orison::syntax::ExpressionSyntax> {};
    scale_arguments.push_back(orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::integer_literal,
        .text = "2",
    });
    auto member_state = orison::lowering::FunctionLoweringState {};
    member_state.immutable_bindings.emplace("value", LoweredExpression {
        .type = "i32",
        .value = "%value",
        .signedness = IntegerSignedness::unsigned_integer,
    });
    member_state.source_type_names.emplace("value", "UInt32");
    auto member_lowering = orison::lowering::LoweringContext {};
    auto member_string_constants = orison::lowering::StringConstantTable {};
    auto member_context = orison::lowering::LoweringEmissionContext {
        .lowering = member_lowering,
        .string_constants = member_string_constants,
        .options = {},
    };
    auto member_failures = orison::lowering::LoweringFailures {};
    auto member_session = orison::lowering::FunctionLoweringSession {
        .state = member_state,
        .failures = member_failures,
    };
    auto member_output = std::ostringstream {};
    auto member_arguments = orison::lowering::lower_member_call_arguments(
        receiver,
        std::span<orison::syntax::ExpressionSyntax const>(scale_arguments),
        scale,
        member_context,
        member_session,
        member_output
    );
    assert(member_arguments.has_value());
    assert(member_arguments->size() == 2);
    assert((*member_arguments)[0].value == "%value");
    assert((*member_arguments)[1].value == "2");
    auto member_result = orison::lowering::emit_value_call("%tmp2", scale, *member_arguments, member_output);
    assert(member_result.type == "i32");
    assert(member_output.str() == "  %tmp2 = call i32 @method.UInt32.scale(i32 %value, i32 2)\n");
    return 0;
}
