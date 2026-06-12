#pragma once

#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/merge_plan.hpp"

#include <optional>
#include <ostream>
#include <string_view>

namespace orison::lowering {

using ConditionalArmLowerer = std::optional<LoweredExpression> (*)(void* context);
using ConditionalBetweenArms = void (*)(void* context);

struct ConditionalLoweringCallbacks {
    void* context = nullptr;
    ConditionalArmLowerer lower_then = nullptr;
    ConditionalBetweenArms between_arms = nullptr;
    ConditionalArmLowerer lower_else = nullptr;
};

enum class ConditionalEmissionFailure {
    none,
    then_arm,
    else_arm,
    branch_mismatch,
};

struct ConditionalEmissionResult {
    std::optional<LoweredExpression> value;
    ConditionalEmissionFailure failure = ConditionalEmissionFailure::none;
    std::optional<BranchMergeMismatch> mismatch;
};

auto emit_conditional_value(
    ConditionalPlan const& plan,
    std::string_view condition,
    FunctionLoweringState& state,
    std::ostream& output,
    ConditionalLoweringCallbacks const& callbacks
) -> ConditionalEmissionResult;

}  // namespace orison::lowering
