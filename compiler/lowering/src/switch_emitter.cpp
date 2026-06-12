#include "orison/lowering/switch_emitter.hpp"

#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"

#include <string>
#include <utility>
#include <vector>

namespace orison::lowering {

auto emit_switch_value(
    SwitchPlan const& plan,
    LoweredExpression const& subject,
    FunctionLoweringState& state,
    std::ostream& output,
    SwitchLoweringCallbacks const& callbacks
) -> SwitchEmissionResult {
    auto switch_targets = std::vector<LlvmSwitchTarget> {};
    switch_targets.reserve(plan.cases.size());
    for (auto const& planned_case : plan.cases) {
        if (planned_case.pattern.has_value()) {
            switch_targets.push_back(LlvmSwitchTarget {
                .value = planned_case.pattern->value,
                .block = planned_case.block,
            });
        }
    }
    emit_llvm_switch(output, subject.type, subject.value, plan.default_block, switch_targets);

    auto incoming_values = std::vector<LoweredExpression> {};
    auto incoming_blocks = std::vector<std::string> {};
    incoming_values.reserve(plan.cases.size());
    incoming_blocks.reserve(plan.cases.size());
    for (auto const& planned_case : plan.cases) {
        emit_llvm_block_label(output, planned_case.block);
        state.current_block = planned_case.block;
        if (callbacks.before_case != nullptr) {
            callbacks.before_case(callbacks.context, planned_case);
        }
        if (callbacks.lower_case == nullptr) {
            return {
                .failure = SwitchEmissionFailure::case_failure,
            };
        }
        auto case_value = callbacks.lower_case(callbacks.context, planned_case);
        if (!case_value.has_value()) {
            return {
                .failure = SwitchEmissionFailure::case_failure,
            };
        }
        auto case_exit_block = state.current_block;
        emit_llvm_branch(output, plan.merge_block);
        incoming_values.push_back(std::move(*case_value));
        incoming_blocks.push_back(std::move(case_exit_block));
    }

    if (!plan.has_default) {
        emit_llvm_block_label(output, plan.fallback_block);
        emit_llvm_unreachable(output);
    }

    if (incoming_values.empty()) {
        return {
            .failure = SwitchEmissionFailure::empty_cases,
        };
    }
    auto merge_inputs = std::vector<BranchMergeInput> {};
    merge_inputs.reserve(incoming_values.size());
    for (auto index = std::size_t {0}; index < incoming_values.size(); ++index) {
        merge_inputs.push_back(BranchMergeInput {
            .value = &incoming_values[index],
            .block = incoming_blocks[index],
        });
    }
    auto merge = plan_branch_merge(merge_inputs);
    if (!merge.plan.has_value()) {
        return {
            .failure = SwitchEmissionFailure::case_type_mismatch,
            .mismatch = std::move(merge.mismatch),
        };
    }

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
