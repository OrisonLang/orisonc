#include "orison/lowering/switch_emitter.hpp"

#include <cassert>
#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

struct CaseContext {
    orison::lowering::FunctionLoweringState* state = nullptr;
    std::vector<orison::lowering::LoweredExpression> values;
    std::size_t next_value = 0;
    std::size_t before_case_count = 0;
    std::optional<std::size_t> failing_case;
};

void before_case(void* opaque, orison::lowering::LoweredSwitchCasePlan const&) {
    ++static_cast<CaseContext*>(opaque)->before_case_count;
}

auto lower_case(
    void* opaque,
    orison::lowering::LoweredSwitchCasePlan const&
) -> std::optional<orison::lowering::LoweredExpression> {
    auto& context = *static_cast<CaseContext*>(opaque);
    auto index = context.next_value++;
    if (context.failing_case == index) {
        return std::nullopt;
    }
    context.state->current_block = "case.exit." + std::to_string(index);
    return context.values[index];
}

auto planned_case(std::string block, std::optional<std::string> pattern)
    -> orison::lowering::LoweredSwitchCasePlan {
    auto result = orison::lowering::LoweredSwitchCasePlan {
        .block = std::move(block),
    };
    if (pattern.has_value()) {
        result.pattern = orison::lowering::LoweredExpression {
            .type = "i32",
            .value = std::move(*pattern),
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        };
    }
    return result;
}

}  // namespace

int main() {
    auto state = orison::lowering::FunctionLoweringState {};
    auto context = CaseContext {
        .state = &state,
        .values = {
            {
                .type = "i32",
                .value = "%first",
                .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
            },
            {
                .type = "i32",
                .value = "%fallback",
                .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
            },
        },
    };
    auto callbacks = orison::lowering::SwitchLoweringCallbacks {
        .context = &context,
        .before_case = before_case,
        .lower_case = lower_case,
    };
    auto output = std::ostringstream {};
    auto result = orison::lowering::emit_switch_value(
        {
            .merge_block = "switch.merge.0",
            .fallback_block = "switch.unreachable.0",
            .default_block = "switch.default.0",
            .has_default = true,
            .cases = {
                planned_case("switch.case.0.0", "1"),
                planned_case("switch.default.0", std::nullopt),
            },
        },
        {
            .type = "i32",
            .value = "%subject",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        },
        state,
        output,
        callbacks
    );
    assert(result.failure == orison::lowering::SwitchEmissionFailure::none);
    assert(result.value.has_value());
    assert(result.value->value == "%tmp0");
    assert(context.before_case_count == 2);
    assert(state.current_block == "switch.merge.0");
    assert(state.next_temporary_index == 1);
    assert(
        output.str() ==
        "  switch i32 %subject, label %switch.default.0 [\n"
        "    i32 1, label %switch.case.0.0\n"
        "  ]\n"
        "switch.case.0.0:\n"
        "  br label %switch.merge.0\n"
        "switch.default.0:\n"
        "  br label %switch.merge.0\n"
        "switch.merge.0:\n"
        "  %tmp0 = phi i32 [%first, %case.exit.0], [%fallback, %case.exit.1]\n"
    );

    context.next_value = 0;
    context.before_case_count = 0;
    output.str({});
    auto no_default = orison::lowering::emit_switch_value(
        {
            .merge_block = "switch.merge.1",
            .fallback_block = "switch.unreachable.1",
            .default_block = "switch.unreachable.1",
            .cases = {
                planned_case("switch.case.1.0", "1"),
            },
        },
        {
            .type = "i32",
            .value = "%subject",
            .signedness = orison::lowering::IntegerSignedness::unsigned_integer,
        },
        state,
        output,
        callbacks
    );
    assert(no_default.value.has_value());
    assert(output.str().find("switch.unreachable.1:\n  unreachable\n") != std::string::npos);

    context.next_value = 0;
    context.failing_case = 0;
    output.str({});
    auto case_failure = orison::lowering::emit_switch_value(
        {
            .merge_block = "switch.merge.2",
            .fallback_block = "switch.unreachable.2",
            .default_block = "switch.default.2",
            .has_default = true,
            .cases = {
                planned_case("switch.default.2", std::nullopt),
            },
        },
        {
            .type = "i32",
            .value = "%subject",
        },
        state,
        output,
        callbacks
    );
    assert(case_failure.failure == orison::lowering::SwitchEmissionFailure::case_failure);

    context.failing_case.reset();
    context.next_value = 0;
    context.values[1].type = "i1";
    context.values[1].signedness = orison::lowering::IntegerSignedness::not_integer;
    output.str({});
    auto mismatch = orison::lowering::emit_switch_value(
        {
            .merge_block = "switch.merge.3",
            .fallback_block = "switch.unreachable.3",
            .default_block = "switch.default.3",
            .has_default = true,
            .cases = {
                planned_case("switch.case.3.0", "1"),
                planned_case("switch.default.3", std::nullopt),
            },
        },
        {
            .type = "i32",
            .value = "%subject",
        },
        state,
        output,
        callbacks
    );
    assert(mismatch.failure == orison::lowering::SwitchEmissionFailure::case_type_mismatch);
    assert(mismatch.mismatch.has_value());
    assert(mismatch.mismatch->expected.type == "i32");
    assert(mismatch.mismatch->actual.type == "i1");

    output.str({});
    auto empty = orison::lowering::emit_switch_value(
        {
            .merge_block = "switch.merge.4",
            .fallback_block = "switch.unreachable.4",
            .default_block = "switch.unreachable.4",
        },
        {
            .type = "i1",
            .value = "%flag",
        },
        state,
        output,
        callbacks
    );
    assert(empty.failure == orison::lowering::SwitchEmissionFailure::empty_cases);
    return 0;
}
