#include "orison/lowering/cleanup_plan_owner.hpp"

#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/syntax/module_parser.hpp"

#include <algorithm>
#include <string>

namespace orison::lowering {

auto owner_has_local_dynamic_array_cleanup_plan(
    std::string_view owner_name,
    FunctionLoweringState const& state
) -> bool {
    auto owner_prefix = std::string {owner_name};
    owner_prefix += ".";
    return std::ranges::any_of(
        state.dynamic_array_local_cleanup_plans,
        [&](DynamicArrayDescriptorCleanupPlan const& plan) {
            return plan.owner_name == owner_name || plan.owner_name.starts_with(owner_prefix);
        }
    );
}

void remove_local_dynamic_array_cleanup_plans_for_owner(
    std::string_view owner_name,
    FunctionLoweringState& state
) {
    auto owner_prefix = std::string {owner_name};
    owner_prefix += ".";
    auto& plans = state.dynamic_array_local_cleanup_plans;
    plans.erase(
        std::remove_if(
            plans.begin(),
            plans.end(),
            [&](DynamicArrayDescriptorCleanupPlan const& plan) {
                return plan.owner_name == owner_name || plan.owner_name.starts_with(owner_prefix);
            }
        ),
        plans.end()
    );
}

void release_moved_owned_cleanup_expression(
    syntax::ExpressionSyntax const& expression,
    FunctionLoweringState& state
) {
    if (expression.kind == syntax::ExpressionKind::name &&
        owner_has_local_dynamic_array_cleanup_plan(expression.text, state)) {
        remove_local_dynamic_array_cleanup_plans_for_owner(expression.text, state);
        mark_owned_binding_consumed(state.ownership_transfers, expression.text);
        return;
    }

    if (expression.kind == syntax::ExpressionKind::ternary && expression.right != nullptr) {
        release_moved_owned_cleanup_expression(*expression.right, state);
    }
    if (expression.kind == syntax::ExpressionKind::ternary && expression.alternate != nullptr) {
        release_moved_owned_cleanup_expression(*expression.alternate, state);
    }
    if (expression.kind != syntax::ExpressionKind::call &&
        expression.kind != syntax::ExpressionKind::array_literal) {
        return;
    }
    for (auto const& argument : expression.arguments) {
        release_moved_owned_cleanup_expression(argument, state);
    }
}

}  // namespace orison::lowering
