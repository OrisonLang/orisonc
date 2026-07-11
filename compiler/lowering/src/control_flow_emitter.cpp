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
#include <utility>

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
                return lower_value_statement_block(
                    arm.statement.nested_statements,
                    arm.expected_llvm_type,
                    arm.expected_signedness,
                    arm.context,
                    arm.session,
                    arm.diagnostics,
                    arm.output,
                    lower_nested_final_control_flow,
                    lower_unit_deferred_cleanup_block
                );
            },
            .between_arms = [](void* opaque) {
                static_cast<ArmContext*>(opaque)->binding_scope.reset();
            },
            .lower_else = [](void* opaque) {
                auto& arm = *static_cast<ArmContext*>(opaque);
                return lower_value_statement_block(
                    arm.statement.alternate_statements,
                    arm.expected_llvm_type,
                    arm.expected_signedness,
                    arm.context,
                    arm.session,
                    arm.diagnostics,
                    arm.output,
                    lower_nested_final_control_flow,
                    lower_unit_deferred_cleanup_block
                );
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
    return result.value;
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
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::invalid_switch_shape,
            "a final switch requires at least one case"
        );
        return std::nullopt;
    }

    auto subject_type = infer_expression_type(statement.expression, context, state);
    if (!subject_type.has_value() || !is_supported_switch_subject(*subject_type, context)) {
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
    auto switch_subject = switch_subject_for_emit(std::move(*subject), context, session, output);

    auto block_index = next_llvm_block_index(state.next_block_index);
    auto planning = plan_switch(statement.switch_cases, *subject_type, context.lowering, block_index);
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
        LoweredExpression const& original_subject;
    };
    auto case_context = CaseContext {
        .expected_llvm_type = expected_llvm_type,
        .expected_signedness = expected_signedness,
        .context = context,
        .session = session,
        .diagnostics = diagnostics,
        .output = output,
        .binding_scope = binding_scope,
        .original_subject = original_subject,
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
                    current.original_subject,
                    current.context,
                    current.session,
                    current.output
                );
            },
            .lower_case = [](void* opaque, LoweredSwitchCasePlan const& planned_case) {
                auto& current = *static_cast<CaseContext*>(opaque);
                return lower_value_statement_block(
                    planned_case.syntax->statements,
                    current.expected_llvm_type,
                    current.expected_signedness,
                    current.context,
                    current.session,
                    current.diagnostics,
                    current.output,
                    lower_nested_final_control_flow,
                    lower_unit_deferred_cleanup_block
                );
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
    std::ostringstream& output
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
