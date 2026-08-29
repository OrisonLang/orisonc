#include "orison/lowering/cleanup_plan_owner.hpp"

#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/syntax/module_parser.hpp"

#include <algorithm>
#include <string>
#include <vector>

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
    LoweringContext const& context,
    FunctionLoweringState& state
) {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto source_type = state.source_type_names.find(expression.text);
        auto has_dynamic_array_source_type = source_type != state.source_type_names.end() &&
            dynamic_array_element_source_type_name(source_type->second).has_value();
        if (has_dynamic_array_source_type ||
            owner_has_local_dynamic_array_cleanup_plan(expression.text, state)) {
            remove_local_dynamic_array_cleanup_plans_for_owner(expression.text, state);
            mark_owned_binding_consumed(state.ownership_transfers, expression.text);
        }
        return;
    }

    auto path = collect_named_aggregate_path(expression);
    if (path.has_value() && path->base_expression != nullptr) {
        auto owner_source_type = state.source_type_names.find(path->base_expression->text);
        if (owner_source_type != state.source_type_names.end()) {
            auto field_names = std::vector<std::string> {};
            field_names.reserve(path->steps.size());
            for (auto const& step : path->steps) {
                if (step.kind != AggregatePathStepKind::member) {
                    field_names.clear();
                    break;
                }
                field_names.push_back(step.field_name);
            }
            auto transfer = owned_record_member_path_transfer(
                path->base_expression->text,
                owner_source_type->second,
                field_names,
                context
            );
            if (transfer.has_value()) {
                remove_local_dynamic_array_cleanup_plans_for_owner(transfer->binding_name, state);
                mark_owned_binding_consumed(state.ownership_transfers, transfer->binding_name);
            }
        }
        return;
    }

    if (expression.kind == syntax::ExpressionKind::ternary && expression.right != nullptr) {
        release_moved_owned_cleanup_expression(*expression.right, context, state);
    }
    if (expression.kind == syntax::ExpressionKind::ternary && expression.alternate != nullptr) {
        release_moved_owned_cleanup_expression(*expression.alternate, context, state);
    }
    if (expression.kind != syntax::ExpressionKind::call &&
        expression.kind != syntax::ExpressionKind::array_literal) {
        return;
    }
    for (auto const& argument : expression.arguments) {
        release_moved_owned_cleanup_expression(argument, context, state);
    }
}

}  // namespace orison::lowering
