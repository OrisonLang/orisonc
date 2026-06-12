#include "orison/lowering/conditional_emitter.hpp"

#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"

#include <array>
#include <utility>

namespace orison::lowering {

auto emit_conditional_value(
    ConditionalPlan const& plan,
    std::string_view condition,
    FunctionLoweringState& state,
    std::ostream& output,
    ConditionalLoweringCallbacks const& callbacks
) -> ConditionalEmissionResult {
    emit_llvm_conditional_branch(output, condition, plan.then_block, plan.else_block);

    emit_llvm_block_label(output, plan.then_block);
    state.current_block = plan.then_block;
    if (callbacks.lower_then == nullptr) {
        return {
            .failure = ConditionalEmissionFailure::then_arm,
        };
    }
    auto then_value = callbacks.lower_then(callbacks.context);
    if (!then_value.has_value()) {
        return {
            .failure = ConditionalEmissionFailure::then_arm,
        };
    }
    auto then_exit_block = state.current_block;
    emit_llvm_branch(output, plan.merge_block);

    emit_llvm_block_label(output, plan.else_block);
    state.current_block = plan.else_block;
    if (callbacks.between_arms != nullptr) {
        callbacks.between_arms(callbacks.context);
    }
    if (callbacks.lower_else == nullptr) {
        return {
            .failure = ConditionalEmissionFailure::else_arm,
        };
    }
    auto else_value = callbacks.lower_else(callbacks.context);
    if (!else_value.has_value()) {
        return {
            .failure = ConditionalEmissionFailure::else_arm,
        };
    }
    auto else_exit_block = state.current_block;

    auto merge_inputs = std::array {
        BranchMergeInput {
            .value = &*then_value,
            .block = then_exit_block,
        },
        BranchMergeInput {
            .value = &*else_value,
            .block = else_exit_block,
        },
    };
    auto merge = plan_branch_merge(merge_inputs);
    if (!merge.plan.has_value()) {
        return {
            .failure = ConditionalEmissionFailure::branch_mismatch,
            .mismatch = std::move(merge.mismatch),
        };
    }

    emit_llvm_branch(output, plan.merge_block);
    emit_llvm_block_label(output, plan.merge_block);
    state.current_block = plan.merge_block;
    auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
    emit_llvm_phi(
        output,
        temporary_name,
        merge.plan->result_type.type,
        merge.plan->incoming
    );
    return {
        .value = LoweredExpression {
            .type = merge.plan->result_type.type,
            .value = std::move(temporary_name),
            .signedness = merge.plan->result_type.signedness,
        },
    };
}

}  // namespace orison::lowering
