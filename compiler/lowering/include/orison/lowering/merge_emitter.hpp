#pragma once

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/merge_plan.hpp"

#include <ostream>
#include <string_view>

namespace orison::lowering {

auto emit_branch_merge_value(
    std::string_view merge_block,
    BranchMergePlan const& plan,
    FunctionLoweringState& state,
    std::ostream& output
) -> LoweredExpression;

}  // namespace orison::lowering
