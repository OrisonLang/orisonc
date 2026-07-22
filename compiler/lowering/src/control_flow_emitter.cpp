#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/conditional_emitter.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_failure_lifecycle.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/maybe_switch_lowering.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/string_constants.hpp"
#include "orison/lowering/switch_emitter.hpp"
#include "orison/lowering/switch_plan.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/lowering/unit_deferred_cleanup.hpp"

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

using EmissionContext = LoweringEmissionContext;

auto lower_final_if_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression>;

auto lower_final_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression>;

auto owned_switch_payload_transfer_names(
    SwitchPlan const& plan,
    syntax::ExpressionSyntax const& subject_expression,
    std::optional<std::string_view> subject_source_type_name,
    LoweringContext const& context
) -> std::vector<std::string> {
    if (!subject_source_type_name.has_value() ||
        subject_expression.kind != syntax::ExpressionKind::name) {
        return {};
    }

    auto names = std::vector<std::string> {};
    auto seen = std::unordered_set<std::string> {};
    for (auto const& planned_case : plan.cases) {
        if (planned_case.syntax == nullptr || planned_case.syntax->is_default) {
            continue;
        }
        auto const& pattern = planned_case.syntax->pattern;
        if (pattern.kind != syntax::ExpressionKind::call ||
            pattern.left == nullptr ||
            pattern.left->kind != syntax::ExpressionKind::name ||
            pattern.arguments.size() != 1 ||
            pattern.arguments.front().kind != syntax::ExpressionKind::name) {
            continue;
        }
        auto transfer = owned_choice_payload_transfer(
            subject_expression.text,
            *subject_source_type_name,
            pattern.left->text,
            pattern.arguments.front().text,
            context
        );
        if (transfer.has_value() && seen.insert(transfer->binding_name).second) {
            names.push_back(std::move(transfer->binding_name));
        }
    }
    return names;
}

void normalize_switch_payload_transfers(
    std::vector<OwnershipTransferState>& states,
    std::vector<std::string> const& payload_transfer_names
) {
    for (auto& state : states) {
        for (auto const& name : payload_transfer_names) {
            mark_owned_binding_consumed(state, name);
        }
    }
}

auto lower_nested_final_control_flow(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_final_if_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output,
            expected_source_type_name
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
            output,
            expected_source_type_name
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
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto& state = session.state;
    auto& failures = session.failures;
    if (statement.kind != syntax::StatementKind::if_statement || statement.nested_statements.empty() ||
        statement.alternate_statements.empty()) {
        record_control_flow_lowering_failure(
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
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::if_condition_failure,
            expression_lowering_failure_detail(failures.expression)
        );
        return std::nullopt;
    }

    auto plan = plan_conditional(
        ConditionalPlanKind::if_statement,
        next_llvm_block_index(state.next_block_index)
    );
    auto binding_scope = BranchBindingScope(state);
    struct ArmContext {
        syntax::StatementSyntax const& statement;
        std::string_view expected_llvm_type;
        IntegerSignedness expected_signedness;
        EmissionContext const& context;
        FunctionLoweringSession& session;
        diagnostics::DiagnosticBag& diagnostics;
        std::ostringstream& output;
        BranchBindingScope& binding_scope;
        std::optional<std::string_view> expected_source_type_name;
        std::vector<OwnershipTransferState> ownership_transfers_by_arm;
    };
    auto arm_context = ArmContext {
        .statement = statement,
        .expected_llvm_type = expected_llvm_type,
        .expected_signedness = expected_signedness,
        .context = context,
        .session = session,
        .diagnostics = diagnostics,
        .output = output,
        .binding_scope = binding_scope,
        .expected_source_type_name = expected_source_type_name,
    };
    auto result = emit_conditional_value(
        plan,
        condition->value,
        state,
        output,
        ConditionalLoweringCallbacks {
            .context = &arm_context,
            .lower_then = [](void* opaque) {
                auto& arm = *static_cast<ArmContext*>(opaque);
                auto value = lower_value_statement_block(
                    arm.statement.nested_statements,
                    arm.expected_llvm_type,
                    arm.expected_signedness,
                    arm.context,
                    arm.session,
                    arm.diagnostics,
                    arm.output,
                    lower_nested_final_control_flow,
                    lower_unit_deferred_cleanup_block,
                    arm.expected_source_type_name
                );
                if (value.has_value()) {
                    arm.ownership_transfers_by_arm.push_back(arm.session.state.ownership_transfers);
                }
                return value;
            },
            .between_arms = [](void* opaque) {
                static_cast<ArmContext*>(opaque)->binding_scope.reset();
            },
            .lower_else = [](void* opaque) {
                auto& arm = *static_cast<ArmContext*>(opaque);
                auto value = lower_value_statement_block(
                    arm.statement.alternate_statements,
                    arm.expected_llvm_type,
                    arm.expected_signedness,
                    arm.context,
                    arm.session,
                    arm.diagnostics,
                    arm.output,
                    lower_nested_final_control_flow,
                    lower_unit_deferred_cleanup_block,
                    arm.expected_source_type_name
                );
                if (value.has_value()) {
                    arm.ownership_transfers_by_arm.push_back(arm.session.state.ownership_transfers);
                }
                return value;
            },
        }
    );
    if (result.failure == ConditionalEmissionFailure::then_arm) {
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::if_then_arm_failure,
            expression_lowering_failure_detail(failures.expression)
        );
        return std::nullopt;
    }
    if (result.failure == ConditionalEmissionFailure::else_arm) {
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::if_else_arm_failure,
            expression_lowering_failure_detail(failures.expression)
        );
        return std::nullopt;
    }
    if (result.failure == ConditionalEmissionFailure::branch_mismatch) {
        auto const& mismatch = *result.mismatch;
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::if_branch_type_mismatch,
            mismatch.expected.type + " versus " + mismatch.actual.type
        );
        return std::nullopt;
    }
    auto merged_transfers =
        merge_ownership_transfer_states(arm_context.ownership_transfers_by_arm);
    if (!merged_transfers.has_value()) {
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::if_branch_ownership_mismatch,
            "owned DynamicArray moves must match across all continuing branches"
        );
        return std::nullopt;
    }
    binding_scope.commit_ownership_transfers(std::move(*merged_transfers));
    return result.value;
}

auto lower_final_switch_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto& state = session.state;
    auto& failures = session.failures;
    if (statement.kind != syntax::StatementKind::switch_statement || statement.switch_cases.empty()) {
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::invalid_switch_shape,
            "a final switch requires at least one case"
        );
        return std::nullopt;
    }

    auto subject_type = infer_expression_type(statement.expression, context, state);
    auto subject_source_type = source_type_name_for_expression(
        statement.expression,
        context.lowering,
        state
    );
    if (!subject_type.has_value() || !is_supported_switch_subject(*subject_type, context, subject_source_type)) {
        record_control_flow_lowering_failure(
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
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::switch_subject_failure,
            expression_lowering_failure_detail(failures.expression)
        );
        return std::nullopt;
    }
    auto original_subject = *subject;
    auto switch_subject = switch_subject_for_emit(
        std::move(*subject),
        context,
        session,
        output,
        subject_source_type
    );

    auto block_index = next_llvm_block_index(state.next_block_index);
    auto planning = plan_switch(
        statement.switch_cases,
        *subject_type,
        context.lowering,
        subject_source_type,
        block_index
    );
    if (!planning.plan.has_value()) {
        record_control_flow_lowering_failure(failures, planning.failure);
        return std::nullopt;
    }
    auto const& plan = *planning.plan;

    auto binding_scope = BranchBindingScope(state);
    struct CaseContext {
        std::string_view expected_llvm_type;
        IntegerSignedness expected_signedness;
        EmissionContext const& context;
        FunctionLoweringSession& session;
        diagnostics::DiagnosticBag& diagnostics;
        std::ostringstream& output;
        BranchBindingScope& binding_scope;
        syntax::ExpressionSyntax const& subject_expression;
        LoweredExpression const& original_subject;
        std::optional<std::string_view> expected_source_type_name;
        std::optional<std::string_view> subject_source_type_name;
        std::vector<OwnershipTransferState> ownership_transfers_by_case;
    };
    auto case_context = CaseContext {
        .expected_llvm_type = expected_llvm_type,
        .expected_signedness = expected_signedness,
        .context = context,
        .session = session,
        .diagnostics = diagnostics,
        .output = output,
        .binding_scope = binding_scope,
        .subject_expression = statement.expression,
        .original_subject = original_subject,
        .expected_source_type_name = expected_source_type_name,
        .subject_source_type_name = subject_source_type,
    };
    auto result = emit_switch_value(
        plan,
        switch_subject,
        state,
        output,
        SwitchLoweringCallbacks {
            .context = &case_context,
            .before_case = [](void* opaque, LoweredSwitchCasePlan const& planned_case) {
                auto& current = *static_cast<CaseContext*>(opaque);
                current.binding_scope.reset();
                bind_switch_payload(
                    planned_case,
                    current.subject_expression,
                    current.original_subject,
                    current.context,
                    current.session,
                    current.output,
                    current.subject_source_type_name
                );
            },
            .lower_case = [](void* opaque, LoweredSwitchCasePlan const& planned_case) {
                auto& current = *static_cast<CaseContext*>(opaque);
                auto value = lower_value_statement_block(
                    planned_case.syntax->statements,
                    current.expected_llvm_type,
                    current.expected_signedness,
                    current.context,
                    current.session,
                    current.diagnostics,
                    current.output,
                    lower_nested_final_control_flow,
                    lower_unit_deferred_cleanup_block,
                    current.expected_source_type_name
                );
                if (value.has_value()) {
                    current.ownership_transfers_by_case.push_back(current.session.state.ownership_transfers);
                }
                return value;
            },
        }
    );
    if (result.failure == SwitchEmissionFailure::case_failure) {
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::switch_case_failure,
            expression_lowering_failure_detail(failures.expression)
        );
        return std::nullopt;
    }
    if (result.failure == SwitchEmissionFailure::empty_cases) {
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::invalid_switch_shape,
            "a final switch requires a value-producing case"
        );
        return std::nullopt;
    }
    if (result.failure == SwitchEmissionFailure::case_type_mismatch) {
        auto detail = result.mismatch.has_value()
            ? result.mismatch->expected.type + " versus " + result.mismatch->actual.type
            : std::string {};
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::switch_case_type_mismatch,
            std::move(detail)
        );
        return std::nullopt;
    }
    normalize_switch_payload_transfers(
        case_context.ownership_transfers_by_case,
        owned_switch_payload_transfer_names(
            plan,
            statement.expression,
            subject_source_type,
            context.lowering
        )
    );
    auto merged_transfers =
        merge_ownership_transfer_states(case_context.ownership_transfers_by_case);
    if (!merged_transfers.has_value()) {
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::switch_case_ownership_mismatch,
            "owned DynamicArray moves must match across all continuing cases"
        );
        return std::nullopt;
    }
    binding_scope.commit_ownership_transfers(std::move(*merged_transfers));
    return result.value;
}

}  // namespace

auto lower_final_control_flow_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    std::optional<std::string_view> expected_source_type_name
) -> std::optional<LoweredExpression> {
    auto& failures = session.failures;
    reset_control_flow_lowering_failure(failures);
    if (statement.kind == syntax::StatementKind::if_statement) {
        return lower_final_if_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output,
            expected_source_type_name
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
            output,
            expected_source_type_name
        );
    }
    return std::nullopt;
}

}  // namespace orison::lowering
