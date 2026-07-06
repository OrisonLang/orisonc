#include "orison/lowering/while_loop_lowering.hpp"

#include "orison/lowering/llvm_names.hpp"

namespace orison::lowering {

auto plan_while_loop_blocks(FunctionLoweringState& state) -> WhileLoopBlockPlan {
    auto const block_index = next_llvm_block_index(state.next_block_index);
    return WhileLoopBlockPlan {
        .condition_block = llvm_block_name("while.condition", block_index),
        .body_block = llvm_block_name("while.body", block_index),
        .exit_block = llvm_block_name("while.exit", block_index),
    };
}

auto while_loop_targets(WhileLoopBlockPlan const& plan, std::size_t defer_cleanup_depth)
    -> LoopTargets {
    return LoopTargets {
        .break_target = plan.exit_block,
        .continue_target = plan.condition_block,
        .defer_cleanup_depth = defer_cleanup_depth,
    };
}

}  // namespace orison::lowering
