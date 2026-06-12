#include "orison/lowering/merge_plan.hpp"

#include <utility>

namespace orison::lowering {

auto plan_branch_merge(
    std::span<BranchMergeInput const> inputs
) -> BranchMergePlanningResult {
    if (inputs.empty() || inputs.front().value == nullptr) {
        return {};
    }

    auto const& expected_value = *inputs.front().value;
    auto plan = BranchMergePlan {
        .result_type = {
            .type = expected_value.type,
            .signedness = expected_value.signedness,
        },
    };
    plan.incoming.reserve(inputs.size());

    for (auto const& input : inputs) {
        if (input.value == nullptr) {
            return {};
        }
        if (input.value->type != expected_value.type ||
            input.value->signedness != expected_value.signedness) {
            return {
                .mismatch = BranchMergeMismatch {
                    .expected = {
                        .type = expected_value.type,
                        .signedness = expected_value.signedness,
                    },
                    .actual = {
                        .type = input.value->type,
                        .signedness = input.value->signedness,
                    },
                },
            };
        }
        plan.incoming.push_back(BranchMergeIncoming {
            .value = input.value->value,
            .block = input.block,
        });
    }
    return {
        .plan = std::move(plan),
    };
}

}  // namespace orison::lowering
