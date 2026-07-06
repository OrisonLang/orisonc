#include "orison/lowering/for_loop_lowering.hpp"

namespace orison::lowering {

auto plan_for_loop_blocks(FunctionLoweringState& state, std::size_t iteration_count)
    -> ForLoopBlockPlan {
    auto const block_index = next_llvm_block_index(state.next_block_index);
    auto plan = ForLoopBlockPlan {
        .exit_block = llvm_block_name("for.exit", block_index),
        .iteration_blocks = {},
    };
    plan.iteration_blocks.reserve(iteration_count);
    for (auto index = std::size_t {0}; index < iteration_count; ++index) {
        plan.iteration_blocks.push_back(llvm_block_name("for.iteration", block_index, index));
    }
    return plan;
}

auto next_for_iteration_target(ForLoopBlockPlan const& plan, std::size_t index)
    -> std::string const& {
    return index + 1 < plan.iteration_blocks.size() ? plan.iteration_blocks[index + 1]
                                                    : plan.exit_block;
}

}  // namespace orison::lowering
