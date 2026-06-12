#include "orison/lowering/merge_emitter.hpp"

#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"

#include <utility>

namespace orison::lowering {

auto emit_branch_merge_value(
    std::string_view merge_block,
    BranchMergePlan const& plan,
    FunctionLoweringState& state,
    std::ostream& output
) -> LoweredExpression {
    emit_llvm_block_label(output, merge_block);
    state.current_block = merge_block;
    auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
    emit_llvm_phi(output, temporary_name, plan.result_type.type, plan.incoming);
    return {
        .type = plan.result_type.type,
        .value = std::move(temporary_name),
        .signedness = plan.result_type.signedness,
    };
}

}  // namespace orison::lowering
