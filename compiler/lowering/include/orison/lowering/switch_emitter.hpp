#pragma once

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/merge_plan.hpp"
#include "orison/lowering/switch_plan.hpp"

#include <optional>
#include <ostream>

namespace orison::lowering {

using SwitchBeforeCase = void (*)(void* context, LoweredSwitchCasePlan const& planned_case);
using SwitchCaseLowerer = std::optional<LoweredExpression> (*)(
    void* context,
    LoweredSwitchCasePlan const& planned_case
);

struct SwitchLoweringCallbacks {
    void* context = nullptr;
    SwitchBeforeCase before_case = nullptr;
    SwitchCaseLowerer lower_case = nullptr;
};

enum class SwitchEmissionFailure {
    none,
    empty_cases,
    case_failure,
    case_type_mismatch,
};

struct SwitchEmissionResult {
    std::optional<LoweredExpression> value;
    SwitchEmissionFailure failure = SwitchEmissionFailure::none;
    std::optional<BranchMergeMismatch> mismatch;
};

auto emit_switch_value(
    SwitchPlan const& plan,
    LoweredExpression const& subject,
    FunctionLoweringState& state,
    std::ostream& output,
    SwitchLoweringCallbacks const& callbacks
) -> SwitchEmissionResult;

}  // namespace orison::lowering
