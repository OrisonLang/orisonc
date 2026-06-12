#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/immutable_binding_scope.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/merge_plan.hpp"
#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/switch_plan.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

using EmissionContext = LoweringEmissionContext;

void record_control_flow_failure(
    LoweringFailures& failures,
    ControlFlowLoweringFailureReason reason,
    std::string detail = {}
) {
    if (failures.control_flow.reason == ControlFlowLoweringFailureReason::none) {
        failures.control_flow = {
            .reason = reason,
            .detail = std::move(detail),
        };
    }
}

auto expression_failure_detail(LoweringFailures const& failures) -> std::string {
    return render_expression_lowering_failure(failures.expression);
}

auto lower_final_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto lower_final_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression>;

auto lower_nested_final_control_flow(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_final_if_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::switch_statement) {
        return lower_final_switch_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    return std::nullopt;
}

auto lower_final_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto& state = session.state;
    auto& failures = session.failures;
    if (statement.kind != syntax::StatementKind::if_statement || statement.nested_statements.empty() ||
        statement.alternate_statements.empty()) {
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::invalid_if_shape,
            "a final if requires non-empty then and else arms"
        );
        return std::nullopt;
    }

    auto condition = lower_expression(
        statement.expression,
        "i1",
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!condition.has_value()) {
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::if_condition_failure,
            expression_failure_detail(failures)
        );
        return std::nullopt;
    }

    auto block_index = next_llvm_block_index(state.next_block_index);
    auto then_block = llvm_block_name("if.then", block_index);
    auto else_block = llvm_block_name("if.else", block_index);
    auto merge_block = llvm_block_name("if.merge", block_index);
    emit_llvm_conditional_branch(output, condition->value, then_block, else_block);

    emit_llvm_block_label(output, then_block);
    state.current_block = then_block;
    auto binding_scope = ImmutableBindingScope(state);
    auto then_value = lower_value_statement_block(
        statement.nested_statements,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_nested_final_control_flow
    );
    if (!then_value.has_value()) {
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::if_then_arm_failure,
            expression_failure_detail(failures)
        );
        return std::nullopt;
    }
    auto then_exit_block = state.current_block;
    emit_llvm_branch(output, merge_block);

    emit_llvm_block_label(output, else_block);
    state.current_block = else_block;
    binding_scope.reset();
    auto else_value = lower_value_statement_block(
        statement.alternate_statements,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_nested_final_control_flow
    );
    if (!else_value.has_value()) {
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::if_else_arm_failure,
            expression_failure_detail(failures)
        );
        return std::nullopt;
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
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::if_branch_type_mismatch,
            then_value->type + " versus " + else_value->type
        );
        return std::nullopt;
    }
    emit_llvm_branch(output, merge_block);

    emit_llvm_block_label(output, merge_block);
    state.current_block = merge_block;
    auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
    emit_llvm_phi(
        output,
        temporary_name,
        merge.plan->result_type.type,
        merge.plan->incoming
    );
    return LoweredExpression {
        .type = merge.plan->result_type.type,
        .value = std::move(temporary_name),
        .signedness = merge.plan->result_type.signedness,
    };
}

auto lower_final_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto& state = session.state;
    auto& failures = session.failures;
    if (statement.kind != syntax::StatementKind::switch_statement || statement.switch_cases.empty()) {
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::invalid_switch_shape,
            "a final switch requires at least one case"
        );
        return std::nullopt;
    }

    auto subject_type = infer_expression_type(statement.expression, context, state);
    if (!subject_type.has_value() ||
        (subject_type->type != "i1" && !is_integer_llvm_type(subject_type->type))) {
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::switch_subject_type_failure
        );
        return std::nullopt;
    }
    auto subject = lower_expression(
        statement.expression,
        subject_type->type,
        subject_type->signedness,
        context,
        session,
        output
    );
    if (!subject.has_value()) {
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::switch_subject_failure,
            expression_failure_detail(failures)
        );
        return std::nullopt;
    }

    auto block_index = next_llvm_block_index(state.next_block_index);
    auto planning = plan_switch(statement.switch_cases, *subject_type, block_index);
    if (!planning.plan.has_value()) {
        record_control_flow_failure(failures, planning.failure);
        return std::nullopt;
    }
    auto const& plan = *planning.plan;

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
    emit_llvm_switch(output, subject->type, subject->value, plan.default_block, switch_targets);

    auto binding_scope = ImmutableBindingScope(state);
    auto incoming_values = std::vector<LoweredExpression> {};
    auto incoming_blocks = std::vector<std::string> {};
    incoming_values.reserve(plan.cases.size());
    incoming_blocks.reserve(plan.cases.size());
    for (auto const& planned_case : plan.cases) {
        emit_llvm_block_label(output, planned_case.block);
        state.current_block = planned_case.block;
        binding_scope.reset();
        auto case_value = lower_value_statement_block(
            planned_case.syntax->statements,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output,
            lower_nested_final_control_flow
        );
        if (!case_value.has_value()) {
            record_control_flow_failure(
                failures,
                ControlFlowLoweringFailureReason::switch_case_failure,
                expression_failure_detail(failures)
            );
            return std::nullopt;
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
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::invalid_switch_shape,
            "a final switch requires a value-producing case"
        );
        return std::nullopt;
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
        auto detail = merge.mismatch.has_value()
            ? merge.mismatch->expected.type + " versus " + merge.mismatch->actual.type
            : std::string {};
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::switch_case_type_mismatch,
            std::move(detail)
        );
        return std::nullopt;
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
    return LoweredExpression {
        .type = merge.plan->result_type.type,
        .value = std::move(temporary_name),
        .signedness = merge.plan->result_type.signedness,
    };
}

}  // namespace

auto lower_final_control_flow_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto& failures = session.failures;
    failures.control_flow = {};
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_final_if_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::switch_statement) {
        return lower_final_switch_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    return std::nullopt;
}

}  // namespace orison::lowering
