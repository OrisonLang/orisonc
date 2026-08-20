#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/conditional_emitter.hpp"
#include "orison/lowering/conditional_plan.hpp"
#include "orison/lowering/dynamic_array_cleanup_plan.hpp"
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

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
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

auto switch_case_payload_binding_names(syntax::ExpressionSyntax const& pattern) -> std::vector<std::string> {
    auto names = std::vector<std::string> {};
    if (pattern.kind != syntax::ExpressionKind::call) {
        return names;
    }
    for (auto const& argument : pattern.arguments) {
        if (argument.kind == syntax::ExpressionKind::name) {
            names.push_back(argument.text);
        }
    }
    return names;
}

auto switch_case_final_expression(syntax::SwitchCaseSyntax const& syntax_case)
    -> syntax::ExpressionSyntax const* {
    if (syntax_case.statements.empty()) {
        return nullptr;
    }
    auto const* statement = syntax_case.statements.back().get();
    if (statement == nullptr) {
        return nullptr;
    }
    if (statement->kind != syntax::StatementKind::return_statement &&
        statement->kind != syntax::StatementKind::expression_statement) {
        return nullptr;
    }
    return &statement->expression;
}

auto stored_choice_payload_owner_names(
    EmissionContext const& context,
    FunctionLoweringSession const& session
) -> std::vector<std::string> {
    auto names = std::vector<std::string> {};
    names.reserve(session.state.source_type_names.size());
    for (auto const& [name, source_type_name] : session.state.source_type_names) {
        if (context.lowering.choices.contains(source_type_name)) {
            names.push_back(name);
        }
    }
    return names;
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
                if (value.has_value() &&
                    arm.expected_source_type_name.has_value() &&
                    is_scalar_or_nonowning_source_type(*arm.expected_source_type_name) &&
                    !emit_choice_dynamic_array_payload_cleanups_for_names(
                        arm.context,
                        arm.session,
                        arm.output,
                        stored_choice_payload_owner_names(arm.context, arm.session)
                    )) {
                    return std::optional<LoweredExpression> {};
                }
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
                if (value.has_value() &&
                    arm.expected_source_type_name.has_value() &&
                    is_scalar_or_nonowning_source_type(*arm.expected_source_type_name) &&
                    !emit_choice_dynamic_array_payload_cleanups_for_names(
                        arm.context,
                        arm.session,
                        arm.output,
                        stored_choice_payload_owner_names(arm.context, arm.session)
                    )) {
                    return std::optional<LoweredExpression> {};
                }
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
        auto detail = std::string {"owned transfers must match across all continuing branches"};
        for (auto const& line : format_branch_local_cleanup_plan_report(
                 plan_branch_local_cleanups(arm_context.ownership_transfers_by_arm)
             )) {
            detail += "\n";
            detail += line;
        }
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::if_branch_ownership_mismatch,
            std::move(detail)
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
        std::size_t case_dynamic_array_cleanup_plan_depth = 0;
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
                current.case_dynamic_array_cleanup_plan_depth =
                    current.session.state.dynamic_array_local_cleanup_plans.size();
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
                auto branch_local_payload_names = switch_case_payload_binding_names(planned_case.syntax->pattern);
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
                    auto const* final_expression = switch_case_final_expression(*planned_case.syntax);
                    if (final_expression != nullptr &&
                        final_expression->kind == syntax::ExpressionKind::name &&
                        current.expected_source_type_name.has_value() &&
                        dynamic_array_element_source_type_name(*current.expected_source_type_name).has_value() &&
                        value->type == std::string {dynamic_array_descriptor_llvm_type()}) {
                        auto source_type = current.session.state.source_type_names.find(final_expression->text);
                        if (source_type != current.session.state.source_type_names.end() &&
                            source_type->second == *current.expected_source_type_name) {
                            mark_owned_binding_consumed(
                                current.session.state.ownership_transfers,
                                final_expression->text
                            );
                        }
                    }
                    auto saved_cleanup_plans = current.session.state.dynamic_array_local_cleanup_plans;
                    auto const cleanup_ordinal_start = current.session.state.next_temporary_index;
                    auto scoped_cleanup_plans = std::vector<DynamicArrayDescriptorCleanupPlan> {
                        saved_cleanup_plans.begin() +
                            static_cast<std::ptrdiff_t>(current.case_dynamic_array_cleanup_plan_depth),
                        saved_cleanup_plans.end(),
                    };
                    for (auto cleanup_plan = scoped_cleanup_plans.begin();
                         cleanup_plan != scoped_cleanup_plans.end();) {
                        if (is_owned_binding_consumed(
                                current.session.state.ownership_transfers,
                                cleanup_plan->owner_name
                            )) {
                            cleanup_plan = scoped_cleanup_plans.erase(cleanup_plan);
                            continue;
                        }
                        ++cleanup_plan;
                    }
                    auto scoped_cleanup_exit_block = std::optional<std::string> {};
                    if (!scoped_cleanup_plans.empty()) {
                        auto const& final_cleanup_plan = scoped_cleanup_plans.back();
                        auto final_cleanup_ordinal = cleanup_ordinal_start + scoped_cleanup_plans.size() - 1;
                        auto label_prefix = final_cleanup_plan.owner_name + ".dynamic_array_cleanup" +
                            std::to_string(final_cleanup_ordinal);
                        scoped_cleanup_exit_block =
                            is_scalar_or_nonowning_source_type(final_cleanup_plan.element_source_type_name)
                            ? label_prefix + ".cleanup.entry"
                            : label_prefix + ".drop.done";
                    }
                    current.session.state.dynamic_array_local_cleanup_plans = std::move(scoped_cleanup_plans);
                    if (!emit_local_dynamic_array_cleanups(
                            current.context,
                            current.session,
                            current.output
                        )) {
                        current.session.state.dynamic_array_local_cleanup_plans = std::move(saved_cleanup_plans);
                        return std::optional<LoweredExpression> {};
                    }
                    current.session.state.dynamic_array_local_cleanup_plans = std::move(saved_cleanup_plans);
                    if (scoped_cleanup_exit_block.has_value()) {
                        current.session.state.current_block = std::move(*scoped_cleanup_exit_block);
                    }
                    auto branch_transfers = current.session.state.ownership_transfers;
                    for (auto const& name : branch_local_payload_names) {
                        branch_transfers.consumed_owned_bindings.erase(name);
                    }
                    current.ownership_transfers_by_case.push_back(std::move(branch_transfers));
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
    if (statement.expression.kind == syntax::ExpressionKind::name) {
        normalize_consumed_owned_descendants(
            case_context.ownership_transfers_by_case,
            consumed_owned_descendant_names(case_context.ownership_transfers_by_case, statement.expression.text)
        );
    }
    auto merged_transfers =
        merge_ownership_transfer_states(case_context.ownership_transfers_by_case);
    if (!merged_transfers.has_value()) {
        auto detail = std::string {"owned transfers must match across all continuing cases"};
        for (auto const& line : format_branch_local_cleanup_plan_report(
                 plan_branch_local_cleanups(case_context.ownership_transfers_by_case)
             )) {
            detail += "\n";
            detail += line;
        }
        record_control_flow_lowering_failure(
            failures,
            ControlFlowLoweringFailureReason::switch_case_ownership_mismatch,
            std::move(detail)
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
