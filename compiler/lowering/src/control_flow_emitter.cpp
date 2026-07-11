#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/conditional_emitter.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_failure_lifecycle.hpp"
#include "orison/lowering/llvm_names.hpp"
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

auto is_maybe_switch_subject(LoweredType const& type) -> bool {
    return type.type.starts_with("{ i1,");
}

auto is_supported_switch_subject(LoweredType const& type) -> bool {
    return type.type == "i1" || is_integer_llvm_type(type.type) || is_maybe_switch_subject(type);
}

auto switch_subject_for_emit(
    LoweredExpression subject,
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> LoweredExpression {
    if (!subject.type.starts_with("{ i1,")) {
        return subject;
    }

    auto tag = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << tag << " = extractvalue " << subject.type << " " << subject.value << ", 0\n";
    return LoweredExpression {
        .type = "i1",
        .value = std::move(tag),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto maybe_payload_type_for_switch_subject(std::string_view type) -> std::optional<std::string> {
    constexpr auto prefix = std::string_view {"{ i1,"};
    if (!type.starts_with(prefix) || !type.ends_with("}")) {
        return std::nullopt;
    }

    auto payload = type.substr(prefix.size(), type.size() - prefix.size() - 1);
    while (!payload.empty() && payload.front() == ' ') {
        payload.remove_prefix(1);
    }
    while (!payload.empty() && payload.back() == ' ') {
        payload.remove_suffix(1);
    }
    return payload.empty() ? std::nullopt : std::optional<std::string> {std::string(payload)};
}

auto maybe_payload_binding_name(syntax::ExpressionSyntax const& pattern) -> std::optional<std::string> {
    if (pattern.kind != syntax::ExpressionKind::call ||
        pattern.left == nullptr ||
        pattern.left->kind != syntax::ExpressionKind::name ||
        pattern.left->text != "Some" ||
        pattern.arguments.size() != 1 ||
        pattern.arguments.front().kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }
    return pattern.arguments.front().text;
}

void bind_maybe_switch_payload(
    LoweredSwitchCasePlan const& planned_case,
    LoweredExpression const& subject,
    EmissionContext const& context,
    FunctionLoweringSession& session,
    std::ostringstream& output
) {
    if (planned_case.syntax == nullptr) {
        return;
    }
    auto payload_type = maybe_payload_type_for_switch_subject(subject.type);
    auto binding_name = maybe_payload_binding_name(planned_case.syntax->pattern);
    if (!payload_type.has_value() || !binding_name.has_value()) {
        return;
    }

    auto payload = next_llvm_temporary_name(session.state.next_temporary_index);
    auto payload_signedness = IntegerSignedness::not_integer;
    if (auto source_type = source_type_name_for_llvm_type(*payload_type, context.lowering)) {
        if (auto lowered_type = lowered_type_for_source_type_name(*source_type, context.lowering)) {
            payload_signedness = lowered_type->signedness;
        }
        session.state.source_type_names[*binding_name] = std::move(*source_type);
    }
    output << "  " << payload << " = extractvalue " << subject.type << " " << subject.value << ", 1\n";
    session.state.immutable_bindings[*binding_name] = LoweredExpression {
        .type = *payload_type,
        .value = std::move(payload),
        .signedness = payload_signedness,
    };
}

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
    if (!subject_type.has_value() || !is_supported_switch_subject(*subject_type)) {
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
    auto switch_subject = switch_subject_for_emit(std::move(*subject), session, output);

    auto block_index = next_llvm_block_index(state.next_block_index);
    auto planning = plan_switch(statement.switch_cases, *subject_type, block_index);
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
                bind_maybe_switch_payload(
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
