#include "orison/lowering/conditional_emitter.hpp"

#include <cassert>
#include <optional>
#include <sstream>
#include <string>

namespace {

struct ArmContext {
    orison::lowering::FunctionLoweringState* state = nullptr;
    orison::lowering::LoweredExpression then_value;
    orison::lowering::LoweredExpression else_value;
    bool fail_then = false;
    bool fail_else = false;
    bool between_arms_called = false;
};

auto lower_then(void* opaque) -> std::optional<orison::lowering::LoweredExpression> {
    auto& context = *static_cast<ArmContext*>(opaque);
    if (context.fail_then) {
        return std::nullopt;
    }
    context.state->current_block = "nested.then.exit";
    return context.then_value;
}

void between_arms(void* opaque) {
    static_cast<ArmContext*>(opaque)->between_arms_called = true;
}

auto lower_else(void* opaque) -> std::optional<orison::lowering::LoweredExpression> {
    auto& context = *static_cast<ArmContext*>(opaque);
    if (context.fail_else) {
        return std::nullopt;
    }
    context.state->current_block = "nested.else.exit";
    return context.else_value;
}

}  // namespace

int main() {
    auto state = orison::lowering::FunctionLoweringState {};
    auto context = ArmContext {
        .state = &state,
        .then_value = {
            .type = "i32",
            .value = "%then",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        },
        .else_value = {
            .type = "i32",
            .value = "%else",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        },
    };
    auto callbacks = orison::lowering::ConditionalLoweringCallbacks {
        .context = &context,
        .lower_then = lower_then,
        .between_arms = between_arms,
        .lower_else = lower_else,
    };
    auto output = std::ostringstream {};
    auto result = orison::lowering::emit_conditional_value(
        {
            .then_block = "if.then.0",
            .else_block = "if.else.0",
            .merge_block = "if.merge.0",
        },
        "%flag",
        state,
        output,
        callbacks
    );
    assert(result.failure == orison::lowering::ConditionalEmissionFailure::none);
    assert(result.value.has_value());
    assert(result.value->type == "i32");
    assert(result.value->value == "%tmp0");
    assert(result.value->signedness == orison::lowering::IntegerSignedness::unsigned_integer);
    assert(context.between_arms_called);
    assert(state.current_block == "if.merge.0");
    assert(state.next_temporary_index == 1);
    assert(
        output.str() ==
        "  br i1 %flag, label %if.then.0, label %if.else.0\n"
        "if.then.0:\n"
        "  br label %if.merge.0\n"
        "if.else.0:\n"
        "  br label %if.merge.0\n"
        "if.merge.0:\n"
        "  %tmp0 = phi i32 [%then, %nested.then.exit], [%else, %nested.else.exit]\n"
    );

    context.fail_then = true;
    context.between_arms_called = false;
    output.str({});
    auto then_failure = orison::lowering::emit_conditional_value(
        {
            .then_block = "ternary.then.1",
            .else_block = "ternary.else.1",
            .merge_block = "ternary.merge.1",
        },
        "%flag",
        state,
        output,
        callbacks
    );
    assert(then_failure.failure == orison::lowering::ConditionalEmissionFailure::then_arm);
    assert(!then_failure.value.has_value());
    assert(!context.between_arms_called);

    context.fail_then = false;
    context.fail_else = true;
    output.str({});
    auto else_failure = orison::lowering::emit_conditional_value(
        {
            .then_block = "ternary.then.2",
            .else_block = "ternary.else.2",
            .merge_block = "ternary.merge.2",
        },
        "%flag",
        state,
        output,
        callbacks
    );
    assert(else_failure.failure == orison::lowering::ConditionalEmissionFailure::else_arm);
    assert(context.between_arms_called);

    context.fail_else = false;
    context.else_value.type = "i1";
    context.else_value.signedness = orison::lowering::IntegerSignedness::not_integer;
    output.str({});
    auto mismatch = orison::lowering::emit_conditional_value(
        {
            .then_block = "ternary.then.3",
            .else_block = "ternary.else.3",
            .merge_block = "ternary.merge.3",
        },
        "%flag",
        state,
        output,
        callbacks
    );
    assert(mismatch.failure == orison::lowering::ConditionalEmissionFailure::branch_mismatch);
    assert(mismatch.mismatch.has_value());
    assert(mismatch.mismatch->expected.type == "i32");
    assert(mismatch.mismatch->actual.type == "i1");
    return 0;
}
