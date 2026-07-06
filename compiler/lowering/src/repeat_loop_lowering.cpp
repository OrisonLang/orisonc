#include "orison/lowering/repeat_loop_lowering.hpp"

#include "orison/lowering/llvm_names.hpp"

namespace orison::lowering {

auto plan_repeat_loop_blocks(FunctionLoweringState& state) -> RepeatLoopBlockPlan {
    auto const block_index = next_llvm_block_index(state.next_block_index);
    return RepeatLoopBlockPlan {
        .body_block = llvm_block_name("repeat.body", block_index),
        .condition_block = llvm_block_name("repeat.condition", block_index),
        .exit_block = llvm_block_name("repeat.exit", block_index),
    };
}

auto repeat_loop_targets(RepeatLoopBlockPlan const& plan, std::size_t defer_cleanup_depth)
    -> LoopTargets {
    return LoopTargets {
        .break_target = plan.exit_block,
        .continue_target = plan.condition_block,
        .defer_cleanup_depth = defer_cleanup_depth,
    };
}

}  // namespace orison::lowering
