#pragma once

#include "orison/lowering/function_lowering_state.hpp"

#include <string_view>

namespace orison::syntax {
struct ExpressionSyntax;
}

namespace orison::lowering {

struct LoweringContext;

auto owner_has_local_dynamic_array_cleanup_plan(
    std::string_view owner_name,
    FunctionLoweringState const& state
) -> bool;

void remove_local_dynamic_array_cleanup_plans_for_owner(
    std::string_view owner_name,
    FunctionLoweringState& state
);

void release_moved_owned_cleanup_expression(
    syntax::ExpressionSyntax const& expression,
    LoweringContext const& context,
    FunctionLoweringState& state
);

}  // namespace orison::lowering
