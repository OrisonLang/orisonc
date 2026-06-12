#pragma once

#include "orison/lowering/lowered_value.hpp"

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct BranchMergeInput {
    LoweredExpression const* value = nullptr;
    std::string_view block;
};

struct BranchMergeIncoming {
    std::string_view value;
    std::string_view block;
};

struct BranchMergePlan {
    LoweredType result_type;
    std::vector<BranchMergeIncoming> incoming;
};

struct BranchMergeMismatch {
    LoweredType expected;
    LoweredType actual;
};

struct BranchMergePlanningResult {
    std::optional<BranchMergePlan> plan;
    std::optional<BranchMergeMismatch> mismatch;
};

auto plan_branch_merge(
    std::span<BranchMergeInput const> inputs
) -> BranchMergePlanningResult;

}  // namespace orison::lowering
