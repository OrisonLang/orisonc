#pragma once

#include <cstddef>
#include <string>

namespace orison::lowering {

enum class ConditionalPlanKind {
    ternary,
    if_statement,
};

struct ConditionalPlan {
    std::string then_block;
    std::string else_block;
    std::string merge_block;
};

auto plan_conditional(ConditionalPlanKind kind, std::size_t block_index) -> ConditionalPlan;

}  // namespace orison::lowering
