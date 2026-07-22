#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/maybe_switch_lowering.hpp"
#include "orison/lowering/ownership_transfer.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/switch_plan.hpp"
#include "orison/syntax/module_parser.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {

template <typename CaseLowerer>
auto lower_nonvalue_switch_statement(
    syntax::StatementSyntax const& statement,
    LoweredType const& subject_type,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    std::string_view unsupported_subject_diagnostic,
    std::string_view unsupported_statement_diagnostic,
    CaseLowerer lower_case
) -> StatementFlow {
    auto subject_source_type = source_type_name_for_expression(
        statement.expression,
        context.lowering,
        session.state
    );
    if (!is_supported_switch_subject(subject_type, context, subject_source_type)) {
        diagnostics.error(statement.line, std::string(unsupported_subject_diagnostic));
        return StatementFlow::failed;
    }

    auto subject = lower_expression(
        statement.expression,
        subject_type.type,
        subject_type.signedness,
        context,
        session,
        output
    );
    if (!subject.has_value()) {
        diagnostics.error(
            statement.line,
            append_expression_lowering_failure(unsupported_subject_diagnostic, session.failures.expression)
        );
        return StatementFlow::failed;
    }
    auto original_subject = *subject;
    auto switch_subject = switch_subject_for_emit(
        std::move(*subject),
        context,
        session,
        output,
        subject_source_type
    );

    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto planning = plan_switch(
        statement.switch_cases,
        subject_type,
        context.lowering,
        subject_source_type,
        block_index
    );
    if (!planning.plan.has_value()) {
        diagnostics.error(
            statement.line,
            append_control_flow_lowering_failure(unsupported_statement_diagnostic, planning.failure)
        );
        return StatementFlow::failed;
    }

    auto const& plan = *planning.plan;
    auto binding_scope = BranchBindingScope(session.state);
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
    emit_llvm_switch(output, switch_subject.type, switch_subject.value, plan.default_block, switch_targets);

    auto all_cases_terminated = true;
    auto fallthrough_ownership_transfers = std::vector<OwnershipTransferState> {};
    for (auto const& planned_case : plan.cases) {
        binding_scope.reset();
        emit_llvm_block_label(output, planned_case.block);
        session.state.current_block = planned_case.block;
        bind_switch_payload(
            planned_case,
            statement.expression,
            original_subject,
            context,
            session,
            output,
            subject_source_type
        );

        auto case_flow = lower_case(planned_case);
        if (case_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (case_flow == StatementFlow::falls_through) {
            fallthrough_ownership_transfers.push_back(session.state.ownership_transfers);
            emit_llvm_branch(output, plan.merge_block);
            all_cases_terminated = false;
        }
    }

    if (!plan.has_default) {
        emit_llvm_block_label(output, plan.fallback_block);
        emit_llvm_unreachable(output);
    }

    if (all_cases_terminated) {
        return StatementFlow::terminated;
    }

    if (statement.expression.kind == syntax::ExpressionKind::name) {
        normalize_consumed_owned_descendants(
            fallthrough_ownership_transfers,
            consumed_owned_descendant_names(fallthrough_ownership_transfers, statement.expression.text)
        );
    }
    auto merged_transfers = merge_ownership_transfer_states(fallthrough_ownership_transfers);
    if (!merged_transfers.has_value()) {
        diagnostics.error(
            statement.line,
            "switch case ownership mismatch: owned DynamicArray moves must match across all continuing cases"
        );
        return StatementFlow::failed;
    }
    binding_scope.commit_ownership_transfers(std::move(*merged_transfers));
    emit_llvm_block_label(output, plan.merge_block);
    session.state.current_block = plan.merge_block;
    return StatementFlow::falls_through;
}

}  // namespace orison::lowering
