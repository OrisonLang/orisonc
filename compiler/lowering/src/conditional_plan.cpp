#include "orison/lowering/conditional_plan.hpp"

#include "orison/lowering/llvm_names.hpp"

namespace orison::lowering {

auto plan_conditional(ConditionalPlanKind kind, std::size_t block_index) -> ConditionalPlan {
    auto is_ternary = kind == ConditionalPlanKind::ternary;
    return {
        .then_block = llvm_block_name(is_ternary ? "ternary.then" : "if.then", block_index),
        .else_block = llvm_block_name(is_ternary ? "ternary.else" : "if.else", block_index),
        .merge_block = llvm_block_name(is_ternary ? "ternary.merge" : "if.merge", block_index),
    };
}

}  // namespace orison::lowering
