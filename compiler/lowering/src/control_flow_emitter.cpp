#include "orison/lowering/control_flow_emitter.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/immutable_binding_scope.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/statement_emitter.hpp"
#include "orison/lowering/string_constants.hpp"
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
    if (then_value->type != else_value->type ||
        then_value->signedness != else_value->signedness) {
        record_control_flow_failure(
            failures,
            ControlFlowLoweringFailureReason::if_branch_type_mismatch,
            then_value->type + " versus " + else_value->type
        );
        return std::nullopt;
    }
    auto else_exit_block = state.current_block;
    emit_llvm_branch(output, merge_block);

    emit_llvm_block_label(output, merge_block);
    state.current_block = merge_block;
    auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
    auto incoming = std::array {
        LlvmPhiIncoming {
            .value = then_value->value,
            .block = then_exit_block,
        },
        LlvmPhiIncoming {
            .value = else_value->value,
            .block = else_exit_block,
        },
    };
    emit_llvm_phi(output, temporary_name, then_value->type, incoming);
    return LoweredExpression {
        .type = then_value->type,
        .value = std::move(temporary_name),
        .signedness = then_value->signedness,
    };
}

auto lowered_switch_pattern(
    syntax::ExpressionSyntax const& pattern,
    LoweredType const& subject_type
) -> std::optional<LoweredExpression> {
    if (auto literal = lower_integer_literal(pattern, subject_type.type, subject_type.signedness)) {
        return literal;
    }
    if (auto literal = lower_boolean_literal(pattern, subject_type.type)) {
        return literal;
    }
    if (pattern.kind != syntax::ExpressionKind::cast || pattern.left == nullptr) {
        return std::nullopt;
    }

    syntax::TypeSyntax target_type {
        .name = pattern.text,
    };
    auto cast_type = llvm_type_for(target_type);
    if (!cast_type.has_value() || *cast_type != subject_type.type) {
        return std::nullopt;
    }
    return lower_integer_literal(*pattern.left, subject_type.type, subject_type.signedness);
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

    struct LoweredSwitchCase {
        syntax::SwitchCaseSyntax const* syntax = nullptr;
        std::string block;
        std::optional<LoweredExpression> pattern;
    };

    auto block_index = next_llvm_block_index(state.next_block_index);
    auto merge_block = llvm_block_name("switch.merge", block_index);
    auto fallback_block = llvm_block_name("switch.unreachable", block_index);
    auto lowered_cases = std::vector<LoweredSwitchCase> {};
    lowered_cases.reserve(statement.switch_cases.size());
    auto default_case_index = std::optional<std::size_t> {};
    auto value_case_index = std::size_t {0};

    for (auto const& switch_case : statement.switch_cases) {
        auto lowered_case = LoweredSwitchCase {
            .syntax = &switch_case,
        };
        if (switch_case.is_default) {
            if (default_case_index.has_value()) {
                record_control_flow_failure(
                    failures,
                    ControlFlowLoweringFailureReason::duplicate_switch_default
                );
                return std::nullopt;
            }
            lowered_case.block = llvm_block_name("switch.default", block_index);
            default_case_index = lowered_cases.size();
        } else {
            lowered_case.block = llvm_block_name("switch.case", block_index, value_case_index++);
            lowered_case.pattern = lowered_switch_pattern(switch_case.pattern, *subject_type);
            if (!lowered_case.pattern.has_value()) {
                record_control_flow_failure(
                    failures,
                    ControlFlowLoweringFailureReason::unsupported_switch_pattern
                );
                return std::nullopt;
            }
        }
        lowered_cases.push_back(std::move(lowered_case));
    }

    auto const& default_block =
        default_case_index.has_value() ? lowered_cases[*default_case_index].block : fallback_block;
    auto switch_targets = std::vector<LlvmSwitchTarget> {};
    switch_targets.reserve(lowered_cases.size());
    for (auto const& lowered_case : lowered_cases) {
        if (lowered_case.pattern.has_value()) {
            switch_targets.push_back(LlvmSwitchTarget {
                .value = lowered_case.pattern->value,
                .block = lowered_case.block,
            });
        }
    }
    emit_llvm_switch(output, subject->type, subject->value, default_block, switch_targets);

    auto binding_scope = ImmutableBindingScope(state);
    auto incoming_values = std::vector<std::pair<LoweredExpression, std::string>> {};
    incoming_values.reserve(lowered_cases.size());
    for (auto const& lowered_case : lowered_cases) {
        emit_llvm_block_label(output, lowered_case.block);
        state.current_block = lowered_case.block;
        binding_scope.reset();
        auto case_value = lower_value_statement_block(
            lowered_case.syntax->statements,
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
        emit_llvm_branch(output, merge_block);
        incoming_values.emplace_back(std::move(*case_value), std::move(case_exit_block));
    }

    if (!default_case_index.has_value()) {
        emit_llvm_block_label(output, fallback_block);
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
    auto const& result_type = incoming_values.front().first;
    for (auto const& [incoming_value, incoming_block] : incoming_values) {
        static_cast<void>(incoming_block);
        if (incoming_value.type != result_type.type || incoming_value.signedness != result_type.signedness) {
            record_control_flow_failure(
                failures,
                ControlFlowLoweringFailureReason::switch_case_type_mismatch,
                result_type.type + " versus " + incoming_value.type
            );
            return std::nullopt;
        }
    }

    emit_llvm_block_label(output, merge_block);
    state.current_block = merge_block;
    auto temporary_name = next_llvm_temporary_name(state.next_temporary_index);
    auto phi_incoming = std::vector<LlvmPhiIncoming> {};
    phi_incoming.reserve(incoming_values.size());
    for (auto const& [incoming_value, incoming_block] : incoming_values) {
        phi_incoming.push_back(LlvmPhiIncoming {
            .value = incoming_value.value,
            .block = incoming_block,
        });
    }
    emit_llvm_phi(output, temporary_name, result_type.type, phi_incoming);
    return LoweredExpression {
        .type = result_type.type,
        .value = std::move(temporary_name),
        .signedness = result_type.signedness,
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
