#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/consumed_descriptor_finalization.hpp"
#include "orison/lowering/dynamic_array_runtime.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/loop_lowering_support.hpp"
#include "orison/lowering/lowering_emission_context.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/type_lowering.hpp"
#include "orison/semantics/drop_model.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace orison::lowering {

struct ForLoopBlockPlan {
    std::string exit_block;
    std::vector<std::string> iteration_blocks;
};

auto plan_for_loop_blocks(FunctionLoweringState& state, std::size_t iteration_count)
    -> ForLoopBlockPlan;

auto next_for_iteration_target(ForLoopBlockPlan const& plan, std::size_t index)
    -> std::string const&;

inline auto dynamic_sequence_for_lowering_enabled(
    DynamicSequenceSourceType const& sequence,
    LlvmIrEmissionOptions const& options
) -> bool {
    return sequence.kind != DynamicSequenceKind::dynamic_array ||
        options.enable_dynamic_array_for_lowering;
}

inline auto emit_view_descriptor_field_projection(
    std::string_view result_name,
    std::string_view descriptor_value_name,
    std::size_t field_index
) -> std::string {
    auto output = std::ostringstream {};
    output << "  " << result_name << " = extractvalue ";
    output << view_descriptor_llvm_type() << " " << descriptor_value_name;
    output << ", " << field_index << "\n";
    return output.str();
}

inline auto expression_references_name(
    syntax::ExpressionSyntax const& expression,
    std::string_view name
) -> bool;

inline auto statement_references_name(
    syntax::StatementSyntax const& statement,
    std::string_view name
) -> bool;

inline auto expression_references_name(
    syntax::ExpressionSyntax const& expression,
    std::string_view name
) -> bool {
    if (expression.kind == syntax::ExpressionKind::name && expression.text == name) {
        return true;
    }
    for (auto const& argument : expression.arguments) {
        if (expression_references_name(argument, name)) {
            return true;
        }
    }
    for (auto const& nested_statement : expression.nested_statements) {
        if (nested_statement != nullptr && statement_references_name(*nested_statement, name)) {
            return true;
        }
    }
    if (expression.left != nullptr && expression_references_name(*expression.left, name)) {
        return true;
    }
    if (expression.right != nullptr && expression_references_name(*expression.right, name)) {
        return true;
    }
    if (expression.alternate != nullptr && expression_references_name(*expression.alternate, name)) {
        return true;
    }
    return false;
}

inline auto statement_references_name(
    syntax::StatementSyntax const& statement,
    std::string_view name
) -> bool {
    if (statement.name == name) {
        return true;
    }
    if (expression_references_name(statement.assignment_target, name) ||
        expression_references_name(statement.expression, name)) {
        return true;
    }
    for (auto const& nested_statement : statement.nested_statements) {
        if (statement_references_name(nested_statement, name)) {
            return true;
        }
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        if (statement_references_name(alternate_statement, name)) {
            return true;
        }
    }
    for (auto const& switch_case : statement.switch_cases) {
        if (expression_references_name(switch_case.pattern, name)) {
            return true;
        }
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr && statement_references_name(*case_statement, name)) {
                return true;
            }
        }
    }
    return false;
}

inline auto later_sibling_statement_references_name(
    FunctionLoweringState const& state,
    std::string_view name
) -> bool {
    for (auto const* statement : state.sibling_statements_after_current) {
        if (statement != nullptr && statement_references_name(*statement, name)) {
            return true;
        }
    }
    for (auto const* statement : state.function_statements_after_current) {
        if (statement != nullptr && statement_references_name(*statement, name)) {
            return true;
        }
    }
    return false;
}

inline auto computed_dynamic_array_has_lowered_local_cleanup_plan(
    std::string_view cleanup_owner_name,
    std::string_view source_type_name,
    FunctionLoweringState const& state
) -> bool {
    for (auto const& cleanup_plan : state.dynamic_array_local_cleanup_plans) {
        if (cleanup_plan.owner_name == cleanup_owner_name &&
            cleanup_plan.source_type_name == source_type_name &&
            cleanup_plan.descriptor_storage_status ==
                DynamicArrayDescriptorStorageStatus::lowered_local_descriptor) {
            return true;
        }
    }
    return false;
}

inline auto computed_dynamic_array_local_cleanup_context_allows_insertion(
    FunctionLoweringState const& state
) -> bool {
    return state.loop_targets.empty();
}

inline auto computed_dynamic_array_local_cleanup_call_insertion_enabled(
    std::string_view cleanup_owner_name,
    std::string_view source_type_name,
    FunctionLoweringState const& state,
    LlvmIrEmissionOptions const& options
) -> bool {
    return options.enable_dynamic_array_cleanup_emission &&
        options.enable_computed_dynamic_array_local_cleanup_call_insertion &&
        computed_dynamic_array_local_cleanup_context_allows_insertion(state) &&
        computed_dynamic_array_has_lowered_local_cleanup_plan(cleanup_owner_name, source_type_name, state) &&
        !later_sibling_statement_references_name(state, cleanup_owner_name);
}

inline auto computed_dynamic_array_local_cleanup_call_insertion_blocked_reason(
    std::string_view cleanup_owner_name,
    std::string_view source_type_name,
    FunctionLoweringState const& state,
    LlvmIrEmissionOptions const& options
) -> std::string {
    if (!options.enable_dynamic_array_cleanup_emission ||
        !options.enable_computed_dynamic_array_local_cleanup_call_insertion ||
        !computed_dynamic_array_has_lowered_local_cleanup_plan(cleanup_owner_name, source_type_name, state)) {
        return {};
    }
    if (!computed_dynamic_array_local_cleanup_context_allows_insertion(state)) {
        return "active loop body";
    }
    if (later_sibling_statement_references_name(state, cleanup_owner_name)) {
        return "later owner use";
    }
    return {};
}

inline void consume_computed_dynamic_array_local_cleanup_plan(
    FunctionLoweringState& state,
    std::string_view cleanup_owner_name,
    [[maybe_unused]] std::string_view source_type_name
) {
    for (auto cleanup_plan = state.dynamic_array_local_cleanup_plans.begin();
         cleanup_plan != state.dynamic_array_local_cleanup_plans.end();) {
        if (cleanup_plan->owner_name == cleanup_owner_name &&
            cleanup_plan->descriptor_storage_status ==
                DynamicArrayDescriptorStorageStatus::lowered_local_descriptor) {
            cleanup_plan = state.dynamic_array_local_cleanup_plans.erase(cleanup_plan);
            continue;
        }
        ++cleanup_plan;
    }
}

inline auto computed_dynamic_array_element_drop_symbol_name(
    std::string_view cleanup_owner_name,
    std::string_view element_source_type_name,
    LlvmIrEmissionOptions const& options
) -> std::optional<std::string> {
    auto expected_owner_name = std::string {cleanup_owner_name} + ".element";
    auto expected_symbol_name = semantics::drop_abi_symbol_name(element_source_type_name);
    for (auto const& authorization : options.semantic_drop_lowering_authorizations) {
        if (authorization.authorized &&
            authorization.site.owner_name == expected_owner_name &&
            authorization.site.source_type_name == element_source_type_name &&
            authorization.site.abi_symbol_name == expected_symbol_name) {
            return expected_symbol_name;
        }
    }
    for (auto const& authorization : options.semantic_drop_lowering_authorizations) {
        if (authorization.authorized &&
            authorization.site.owner_name.ends_with(".element") &&
            authorization.site.source_type_name == element_source_type_name &&
            authorization.site.abi_symbol_name == expected_symbol_name) {
            return expected_symbol_name;
        }
    }
    return std::nullopt;
}

inline auto computed_dynamic_array_drop_label_prefix(std::string_view name_prefix) -> std::string {
    auto label_prefix = std::string {name_prefix};
    if (!label_prefix.empty() && label_prefix.front() == '%') {
        label_prefix.erase(label_prefix.begin());
    }
    return label_prefix;
}

inline auto emit_computed_dynamic_array_element_drop_walk(
    DynamicArrayConstructionPlan const& plan,
    std::string_view data_pointer_name,
    std::string_view length_name,
    std::string_view name_prefix,
    std::string_view entry_block_name,
    std::string_view drop_symbol_name
) -> std::string {
    auto output = std::ostringstream {};
    auto prefix = std::string {name_prefix};
    auto label_prefix = computed_dynamic_array_drop_label_prefix(name_prefix);
    output << "  br label %" << label_prefix << ".drop.walk\n";
    output << label_prefix << ".drop.walk:\n";
    output << "  " << prefix << ".drop.index = phi i64 [ 0, %" << entry_block_name << " ],";
    output << " [ " << prefix << ".drop.next, %" << label_prefix << ".drop.body ]\n";
    output << "  " << prefix << ".drop.more = icmp ult i64 " << prefix << ".drop.index";
    output << ", " << length_name << "\n";
    output << "  br i1 " << prefix << ".drop.more";
    output << ", label %" << label_prefix << ".drop.body";
    output << ", label %" << label_prefix << ".drop.done\n";
    output << label_prefix << ".drop.body:\n";
    output << emit_dynamic_array_element_address(
        plan,
        prefix + ".drop.element.addr",
        data_pointer_name,
        prefix + ".drop.index"
    );
    output << "  ; drop element " << plan.element_source_type_name;
    output << " at " << prefix << ".drop.element.addr using " << drop_symbol_name << "\n";
    output << "  call void @" << drop_symbol_name << "(ptr " << prefix << ".drop.element.addr)\n";
    output << "  " << prefix << ".drop.next = add i64 " << prefix << ".drop.index, 1\n";
    output << "  br label %" << label_prefix << ".drop.walk\n";
    output << label_prefix << ".drop.done:\n";
    return output.str();
}

template <typename LowerBody>
auto lower_sequence_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    LowerBody&& lower_body
) -> StatementFlow {
    auto source_type_name =
        source_type_name_for_expression(statement.expression, context.lowering, session.state);
    if (!source_type_name.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering dynamic-sequence for statements currently requires a typed iterable"
        );
        return StatementFlow::failed;
    }

    auto sequence = dynamic_sequence_source_type(*source_type_name);
    if (!sequence.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering dynamic-sequence for statements currently requires a DynamicArray or View iterable"
        );
        return StatementFlow::failed;
    }
    if (!dynamic_sequence_for_lowering_enabled(*sequence, context.options)) {
        diagnostics.error(
            statement.line,
            "lowering DynamicArray for statements currently requires explicit production enablement"
        );
        return StatementFlow::failed;
    }

    auto element_type = lowered_type_for_source_type_name(sequence->element_source_type_name, context.lowering);
    if (!element_type.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering dynamic-sequence for statements currently requires a lowerable element type"
        );
        return StatementFlow::failed;
    }

    auto dynamic_array_plan = plan_dynamic_array_iterable_descriptor(
        statement.expression,
        context.lowering,
        session.state
    );
    auto named_iterable = statement.expression.kind == syntax::ExpressionKind::name
        ? std::optional<std::string> {statement.expression.text}
        : std::nullopt;
    auto storage = named_iterable.has_value()
        ? aggregate_storage_for_name(*named_iterable, session.state)
        : std::optional<std::string> {};
    if (sequence->kind == DynamicSequenceKind::dynamic_array && !dynamic_array_plan.can_lower_now) {
        auto saved_computed_for_unique_suffix = session.state.computed_dynamic_array_for_unique_suffix;
        auto const computed_dynamic_array_for_lowering_enabled =
            context.options.enable_dynamic_array_for_lowering;
        if (computed_dynamic_array_for_lowering_enabled) {
            session.state.computed_dynamic_array_for_unique_suffix =
                "." + std::to_string(next_llvm_block_index(session.state.next_block_index));
        }
        auto computed_production_emission_gate_plan =
            plan_computed_dynamic_array_iterable_production_emission_gate(
                statement.expression,
                context.lowering,
                session.state
            );
        session.state.computed_dynamic_array_for_unique_suffix = std::move(saved_computed_for_unique_suffix);
        if (
            computed_dynamic_array_for_lowering_enabled &&
            computed_dynamic_array_iterable_production_emission_gate_ready(computed_production_emission_gate_plan) &&
            computed_dynamic_array_iterable_cleanup_transition_ready(computed_production_emission_gate_plan)
        ) {
            auto const& loop_exit_plan = computed_production_emission_gate_plan.loop_exit_cleanup_plan;
            auto const& loop_sequence_plan = loop_exit_plan.loop_render_sequence_plan;
            auto const& loop_continue_plan = loop_sequence_plan.loop_continue_render_plan;
            auto const& element_load_plan = loop_continue_plan.element_load_render_plan;
            auto const& element_address_plan = element_load_plan.element_address_render_plan;
            auto const& loop_control_plan = element_address_plan.loop_control_render_plan;
            auto const& descriptor_plan = loop_control_plan.descriptor_render_plan;
            auto const& cleanup_sequence_plan = descriptor_plan.cleanup_sequence_plan;
            auto const production_local_cleanup_calls_enabled =
                computed_dynamic_array_local_cleanup_call_insertion_enabled(
                    cleanup_sequence_plan.cleanup_owner_name,
                    cleanup_sequence_plan.source_type_name,
                    session.state,
                    context.options
                );
            auto const cleanup_call_authorization_origin =
                production_local_cleanup_calls_enabled ?
                    ComputedDynamicArrayCleanupCallAuthorizationOrigin::production_local_cleanup_plan :
                    (context.options.fixture_authorize_computed_dynamic_array_cleanup_calls ?
                         ComputedDynamicArrayCleanupCallAuthorizationOrigin::explicit_test_seam :
                         ComputedDynamicArrayCleanupCallAuthorizationOrigin::none);
            auto const computed_cleanup_calls_enabled =
                cleanup_call_authorization_origin != ComputedDynamicArrayCleanupCallAuthorizationOrigin::none;
            auto const cleanup_calls_blocked_reason = computed_cleanup_calls_enabled ? std::string {} :
                computed_dynamic_array_local_cleanup_call_insertion_blocked_reason(
                    cleanup_sequence_plan.cleanup_owner_name,
                    cleanup_sequence_plan.source_type_name,
                    session.state,
                    context.options
                );

            auto acquisition_handoff = ComputedDynamicArrayCleanupStateHandoff {
                .kind = ComputedDynamicArrayCleanupStateHandoffKind::acquire,
                .operation_name = cleanup_sequence_plan.loop_entry_cleanup_operation_name,
                .source_owner_name = cleanup_sequence_plan.cleanup_owner_name,
                .target_owner_name = cleanup_sequence_plan.loop_entry_cleanup_owner_name,
                .cleanup_calls_enabled = computed_cleanup_calls_enabled,
                .cleanup_call_authorization_origin = cleanup_call_authorization_origin,
                .cleanup_calls_blocked_reason = cleanup_calls_blocked_reason,
            };
            output << render_computed_dynamic_array_cleanup_state_handoff(acquisition_handoff);
            if (!context.options.suppress_computed_dynamic_array_cleanup_handoff_metadata) {
                session.state.computed_dynamic_array_inserted_cleanup_handoffs.push_back(
                    std::move(acquisition_handoff)
                );
            }
            for (auto const& line : descriptor_plan.rendered_ir) {
                output << line;
            }
            for (auto const& line : loop_control_plan.rendered_ir) {
                output << line;
            }

            auto loop_scope = BranchBindingScope(session.state);
            emit_llvm_block_label(output, loop_sequence_plan.body_block_name);
            session.state.current_block = loop_sequence_plan.body_block_name;
            for (auto const& line : element_address_plan.rendered_ir) {
                output << line;
            }
            for (auto const& line : element_load_plan.rendered_ir) {
                output << line;
            }
            session.state.immutable_bindings[statement.name] = LoweredExpression {
                .type = element_type->type,
                .value = element_load_plan.item_value_name,
                .signedness = element_type->signedness,
            };
            session.state.source_type_names[statement.name] = sequence->element_source_type_name;
            bind_addressable_aggregate_value(
                statement.name,
                session.state.immutable_bindings.at(statement.name),
                session,
                output
            );

            auto loop_targets = LoopTargets {
                .break_target = loop_exit_plan.exit_block_name,
                .continue_target = loop_continue_plan.continue_block_name,
                .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
            };
            [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
            auto body_flow = lower_body();
            if (body_flow == StatementFlow::failed) {
                return StatementFlow::failed;
            }
            if (body_flow == StatementFlow::falls_through) {
                emit_llvm_branch(output, loop_continue_plan.continue_block_name);
            }

            for (auto const& line : loop_continue_plan.rendered_ir) {
                output << line;
            }
            output << loop_exit_plan.exit_block_name << ":\n";
            auto resumption_handoff = ComputedDynamicArrayCleanupStateHandoff {
                .kind = ComputedDynamicArrayCleanupStateHandoffKind::resume,
                .operation_name = loop_exit_plan.cleanup_resumption_operation_name,
                .source_owner_name = loop_exit_plan.loop_entry_cleanup_owner_name,
                .target_owner_name = loop_exit_plan.loop_exit_cleanup_owner_name,
                .cleanup_calls_enabled = computed_cleanup_calls_enabled,
                .cleanup_call_authorization_origin = cleanup_call_authorization_origin,
                .cleanup_calls_blocked_reason = cleanup_calls_blocked_reason,
            };
            output << render_computed_dynamic_array_cleanup_state_handoff(resumption_handoff);
            if (!context.options.suppress_computed_dynamic_array_cleanup_handoff_metadata) {
                session.state.computed_dynamic_array_inserted_cleanup_handoffs.push_back(
                    std::move(resumption_handoff)
                );
            }
            auto const element_size_bytes =
                lowered_type_size_bytes(element_type->type, context.lowering);
            auto cleanup_call_operands = std::optional<ComputedDynamicArrayCleanupCallOperands> {};
            if (element_size_bytes.has_value()) {
                cleanup_call_operands = ComputedDynamicArrayCleanupCallOperands {
                    .cleanup_operation_name = loop_exit_plan.cleanup_resumption_operation_name,
                    .data_pointer_name = descriptor_plan.data_pointer_name,
                    .element_size_bytes = *element_size_bytes,
                    .capacity_name = descriptor_plan.capacity_name,
                    .descriptor_storage_name = descriptor_plan.descriptor_storage_name,
                };
            }
            auto cleanup_continuation_block = loop_exit_plan.exit_block_name;
            if (computed_dynamic_array_cleanup_call_insertion_capability(context.options).enabled &&
                computed_cleanup_calls_enabled) {
                if (element_size_bytes.has_value()) {
                    auto cleanup_call_plan = DynamicArrayConstructionPlan {
                        .owner_name = cleanup_sequence_plan.cleanup_owner_name,
                        .source_type_name = std::string {*source_type_name},
                        .element_source_type_name = sequence->element_source_type_name,
                        .element_llvm_type = element_type->type,
                        .element_size_bytes = *element_size_bytes,
                        .operation = DynamicArrayRuntimeOperation::deallocate,
                    };
                    auto element_drop_symbol_name = computed_dynamic_array_element_drop_symbol_name(
                        cleanup_sequence_plan.cleanup_owner_name,
                        sequence->element_source_type_name,
                        context.options
                    );
                    if (!is_scalar_or_nonowning_source_type(sequence->element_source_type_name) &&
                        !element_drop_symbol_name.has_value()) {
                        diagnostics.error(
                            statement.line,
                            "lowering computed DynamicArray cleanup for owned element type " +
                                sequence->element_source_type_name + " requires authorized element drop"
                        );
                        return StatementFlow::failed;
                    }
                    if (element_drop_symbol_name.has_value()) {
                        auto drop_walk_prefix = "%" + cleanup_sequence_plan.cleanup_owner_name +
                            ".computed_dynamic_array_cleanup" +
                            std::to_string(session.state.next_temporary_index++);
                        output << emit_computed_dynamic_array_element_drop_walk(
                            cleanup_call_plan,
                            descriptor_plan.data_pointer_name,
                            descriptor_plan.length_name,
                            drop_walk_prefix,
                            loop_exit_plan.exit_block_name,
                            *element_drop_symbol_name
                        );
                        cleanup_continuation_block =
                            computed_dynamic_array_drop_label_prefix(drop_walk_prefix) + ".drop.done";
                    }
                    output << emit_dynamic_array_deallocation_call(
                        cleanup_call_plan,
                        descriptor_plan.data_pointer_name,
                        descriptor_plan.capacity_name
                    );
                    if (cleanup_call_operands.has_value()) {
                        cleanup_call_operands->cleanup_call_inserted = true;
                    }
                    auto finalization_plan = plan_consumed_descriptor_finalization(
                        cleanup_sequence_plan.cleanup_owner_name,
                        descriptor_plan.descriptor_storage_name,
                        loop_exit_plan.cleanup_resumption_operation_name
                    );
                    auto const finalization_readiness =
                        plan_consumed_descriptor_finalization_readiness(finalization_plan);
                    if (finalization_readiness.ready) {
                        output << emit_dynamic_array_descriptor_finalization(
                            finalization_plan.descriptor_storage_name
                        );
                        if (cleanup_call_operands.has_value()) {
                            cleanup_call_operands->descriptor_finalized = true;
                        }
                        consume_computed_dynamic_array_local_cleanup_plan(
                            session.state,
                            cleanup_sequence_plan.cleanup_owner_name,
                            cleanup_sequence_plan.source_type_name
                        );
                    }
                }
            }
            if (cleanup_call_operands.has_value() &&
                !context.options.suppress_computed_dynamic_array_cleanup_operand_metadata) {
                session.state.computed_dynamic_array_cleanup_call_operands.push_back(
                    std::move(*cleanup_call_operands)
                );
            }
            session.state.current_block = cleanup_continuation_block;
            return StatementFlow::falls_through;
        }

        auto diagnostic_detail = dynamic_array_iterable_descriptor_plan_report(dynamic_array_plan);
        auto computed_ownership_plan = plan_computed_dynamic_array_iterable_ownership_transfer(
            statement.expression,
            context.lowering,
            session.state
        );
        if (computed_ownership_plan.kind !=
            ComputedDynamicArrayIterableOwnershipPlanKind::not_computed_dynamic_array) {
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_ownership_plan_report(
                computed_ownership_plan
            );
            auto computed_handoff_plan = plan_computed_dynamic_array_iterable_descriptor_handoff(
                statement.expression,
                context.lowering,
                session.state
            );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_descriptor_handoff_plan_report(
                computed_handoff_plan
            );
            auto computed_cleanup_sequence_plan =
                plan_computed_dynamic_array_iterable_cleanup_sequence(
                    statement.expression,
                    context.lowering,
                    session.state
                );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_cleanup_sequence_plan_report(
                computed_cleanup_sequence_plan
            );
            auto computed_descriptor_render_plan =
                plan_computed_dynamic_array_iterable_descriptor_render(
                    statement.expression,
                    context.lowering,
                    session.state
                );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_descriptor_render_plan_report(
                computed_descriptor_render_plan
            );
            auto computed_loop_control_render_plan =
                plan_computed_dynamic_array_iterable_loop_control_render(
                    statement.expression,
                    context.lowering,
                    session.state
                );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_loop_control_render_plan_report(
                computed_loop_control_render_plan
            );
            auto computed_element_address_render_plan =
                plan_computed_dynamic_array_iterable_element_address_render(
                    statement.expression,
                    context.lowering,
                    session.state
                );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_element_address_render_plan_report(
                computed_element_address_render_plan
            );
            auto computed_element_load_render_plan =
                plan_computed_dynamic_array_iterable_element_load_render(
                    statement.expression,
                    context.lowering,
                    session.state
                );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_element_load_render_plan_report(
                computed_element_load_render_plan
            );
            auto computed_loop_continue_render_plan =
                plan_computed_dynamic_array_iterable_loop_continue_render(
                    statement.expression,
                    context.lowering,
                    session.state
                );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_loop_continue_render_plan_report(
                computed_loop_continue_render_plan
            );
            auto computed_loop_render_sequence_plan =
                plan_computed_dynamic_array_iterable_loop_render_sequence(
                    statement.expression,
                    context.lowering,
                    session.state
                );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_loop_render_sequence_plan_report(
                computed_loop_render_sequence_plan
            );
            auto computed_loop_exit_cleanup_plan =
                plan_computed_dynamic_array_iterable_loop_exit_cleanup(
                    statement.expression,
                    context.lowering,
                    session.state
                );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_loop_exit_cleanup_plan_report(
                computed_loop_exit_cleanup_plan
            );
            diagnostic_detail += "; ";
            diagnostic_detail += computed_dynamic_array_iterable_production_emission_gate_plan_report(
                computed_production_emission_gate_plan
            );
        }
        diagnostics.error(
            statement.line,
            "lowering DynamicArray for statements currently requires a named descriptor iterable: " +
                diagnostic_detail
        );
        return StatementFlow::failed;
    }
    if (sequence->kind == DynamicSequenceKind::dynamic_array) {
        storage = dynamic_array_plan.descriptor_storage;
    }

    auto element_llvm_type = llvm_type_for_source_type_name(sequence->element_source_type_name, context.lowering);
    if (!element_llvm_type.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering dynamic-sequence for statements currently requires descriptor metadata"
        );
        return StatementFlow::failed;
    }

    auto prefix_owner = named_iterable.value_or("computed");
    auto prefix = "%" + prefix_owner + ".sequence_for" +
        std::to_string(session.state.next_temporary_index++);
    auto incoming_block = session.state.current_block;
    auto block_index = next_llvm_block_index(session.state.next_block_index);
    auto condition_block = llvm_block_name("for.condition", block_index);
    auto body_block = llvm_block_name("for.body", block_index);
    auto continue_block = llvm_block_name("for.continue", block_index);
    auto exit_block = llvm_block_name("for.exit", block_index);

    if (sequence->kind == DynamicSequenceKind::dynamic_array) {
        output << emit_dynamic_array_descriptor_load(prefix + ".descriptor", *storage);
        output << emit_dynamic_array_descriptor_field_projection(
            prefix + ".data",
            prefix + ".descriptor",
            DynamicArrayDescriptorField::data
        );
        output << emit_dynamic_array_descriptor_field_projection(
            prefix + ".length",
            prefix + ".descriptor",
            DynamicArrayDescriptorField::length
        );
    } else if (storage.has_value()) {
        output << "  " << prefix << ".descriptor = load " << view_descriptor_llvm_type();
        output << ", ptr " << *storage << "\n";
        output << emit_view_descriptor_field_projection(prefix + ".data", prefix + ".descriptor", 0);
        output << emit_view_descriptor_field_projection(prefix + ".length", prefix + ".descriptor", 1);
    } else {
        auto lowered_descriptor = lower_expression(
            statement.expression,
            view_descriptor_llvm_type(),
            IntegerSignedness::not_integer,
            context,
            session,
            output,
            std::string_view {*source_type_name}
        );
        if (!lowered_descriptor.has_value()) {
            diagnostics.error(
                statement.line,
                "lowering View for statements currently requires a lowerable descriptor expression"
            );
            return StatementFlow::failed;
        }
        output << emit_view_descriptor_field_projection(prefix + ".data", lowered_descriptor->value, 0);
        output << emit_view_descriptor_field_projection(prefix + ".length", lowered_descriptor->value, 1);
    }
    emit_llvm_branch(output, condition_block);

    emit_llvm_block_label(output, condition_block);
    session.state.current_block = condition_block;
    output << "  " << prefix << ".index = phi i64 [ 0, %" << incoming_block << " ], [ "
           << prefix << ".next.index, %" << continue_block << " ]\n";
    output << emit_dynamic_array_bounds_check(
        prefix + ".more",
        prefix + ".index",
        prefix + ".length",
        DynamicArrayBoundsCheckKind::index_within_length
    );
    emit_llvm_conditional_branch(output, prefix + ".more", body_block, exit_block);

    auto loop_scope = BranchBindingScope(session.state);
    emit_llvm_block_label(output, body_block);
    session.state.current_block = body_block;
    output << "  " << prefix << ".element.addr = getelementptr " << *element_llvm_type;
    output << ", ptr " << prefix << ".data, i64 " << prefix << ".index\n";
    output << "  " << prefix << ".value = load " << *element_llvm_type;
    output << ", ptr " << prefix << ".element.addr\n";
    session.state.immutable_bindings[statement.name] = LoweredExpression {
        .type = element_type->type,
        .value = prefix + ".value",
        .signedness = element_type->signedness,
    };
    session.state.source_type_names[statement.name] = sequence->element_source_type_name;
    bind_addressable_aggregate_value(
        statement.name,
        session.state.immutable_bindings.at(statement.name),
        session,
        output
    );

    auto loop_targets = LoopTargets {
        .break_target = exit_block,
        .continue_target = continue_block,
        .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
    };
    [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
    auto body_flow = lower_body();
    if (body_flow == StatementFlow::failed) {
        return StatementFlow::failed;
    }
    if (body_flow == StatementFlow::falls_through) {
        emit_llvm_branch(output, continue_block);
    }

    emit_llvm_block_label(output, continue_block);
    session.state.current_block = continue_block;
    output << "  " << prefix << ".next.index = add i64 " << prefix << ".index, 1\n";
    emit_llvm_branch(output, condition_block);

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return StatementFlow::falls_through;
}

template <typename LowerBody>
auto lower_fixed_array_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    LowerBody&& lower_body
) -> StatementFlow {
    auto source_type_name =
        source_type_name_for_expression(statement.expression, context.lowering, session.state);
    if (!source_type_name.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires an array literal or fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto iterable_type = lowered_type_for_source_type_name(*source_type_name, context.lowering);
    if (!iterable_type.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires a fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto array_type = parse_llvm_array_type(iterable_type->type);
    if (!array_type.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires a fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto element_source_type_name = array_element_source_type_name(*source_type_name);
    if (!element_source_type_name.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires a named fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto element_type = lowered_type_for_source_type_name(*element_source_type_name, context.lowering);
    if (!element_type.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering for statements currently requires a fixed-size array iterable"
        );
        return StatementFlow::failed;
    }

    auto lowered_iterable = lower_expression(
        statement.expression,
        iterable_type->type,
        IntegerSignedness::not_integer,
        context,
        session,
        output
    );
    if (!lowered_iterable.has_value()) {
        diagnostics.error(
            statement.expression.line,
            append_expression_lowering_failure(
                "lowering for statements currently requires a fixed-size array iterable",
                session.failures.expression
            )
        );
        return StatementFlow::failed;
    }

    auto block_plan = plan_for_loop_blocks(session.state, array_type->length);
    if (block_plan.iteration_blocks.empty()) {
        emit_llvm_branch(output, block_plan.exit_block);
        emit_llvm_block_label(output, block_plan.exit_block);
        session.state.current_block = block_plan.exit_block;
        return StatementFlow::falls_through;
    }

    auto loop_scope = BranchBindingScope(session.state);
    emit_llvm_branch(output, block_plan.iteration_blocks.front());

    for (auto index = std::size_t {0}; index < array_type->length; ++index) {
        loop_scope.reset();
        emit_llvm_block_label(output, block_plan.iteration_blocks[index]);
        session.state.current_block = block_plan.iteration_blocks[index];

        auto item_name = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << item_name << " = extractvalue " << lowered_iterable->type << " "
               << lowered_iterable->value << ", " << index << "\n";
        session.state.immutable_bindings[statement.name] = LoweredExpression {
            .type = element_type->type,
            .value = item_name,
            .signedness = element_type->signedness,
        };
        session.state.source_type_names[statement.name] = *element_source_type_name;
        bind_addressable_aggregate_value(
            statement.name,
            session.state.immutable_bindings.at(statement.name),
            session,
            output
        );

        auto loop_targets = LoopTargets {
            .break_target = block_plan.exit_block,
            .continue_target = next_for_iteration_target(block_plan, index),
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        };
        [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
        auto body_flow = lower_body();
        if (body_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (body_flow == StatementFlow::falls_through) {
            emit_llvm_branch(output, next_for_iteration_target(block_plan, index));
        }
    }

    emit_llvm_block_label(output, block_plan.exit_block);
    session.state.current_block = block_plan.exit_block;
    return StatementFlow::falls_through;
}

template <typename InferElementType, typename LowerBody>
auto lower_array_literal_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    InferElementType&& infer_element_type,
    LowerBody&& lower_body
) -> StatementFlow {
    auto element_type = std::optional<LoweredType> {};
    for (auto const& element : statement.expression.arguments) {
        auto inferred = infer_element_type(element, context, session.state);
        if (!inferred.has_value()) {
            continue;
        }
        if (!element_type.has_value()) {
            element_type = *inferred;
            continue;
        }
        if (element_type->type != inferred->type || element_type->signedness != inferred->signedness) {
            diagnostics.error(statement.line, "lowering for statements currently requires a uniform iterable type");
            return StatementFlow::failed;
        }
    }
    if (!element_type.has_value()) {
        element_type = LoweredType {
            .type = "i64",
            .signedness = IntegerSignedness::signed_integer,
        };
    }

    auto block_plan = plan_for_loop_blocks(session.state, statement.expression.arguments.size());
    if (block_plan.iteration_blocks.empty()) {
        emit_llvm_branch(output, block_plan.exit_block);
        emit_llvm_block_label(output, block_plan.exit_block);
        session.state.current_block = block_plan.exit_block;
        return StatementFlow::falls_through;
    }

    auto loop_scope = BranchBindingScope(session.state);
    emit_llvm_branch(output, block_plan.iteration_blocks.front());

    for (auto index = std::size_t {0}; index < statement.expression.arguments.size(); ++index) {
        loop_scope.reset();
        emit_llvm_block_label(output, block_plan.iteration_blocks[index]);
        session.state.current_block = block_plan.iteration_blocks[index];

        auto expected_source_type_name = source_type_name_for_expression(
            statement.expression.arguments[index],
            context.lowering,
            session.state
        );
        auto expected_source_type = expected_source_type_name.has_value()
            ? std::optional<std::string_view> {*expected_source_type_name}
            : std::optional<std::string_view> {};
        auto lowered_item = lower_expression(
            statement.expression.arguments[index],
            element_type->type,
            element_type->signedness,
            context,
            session,
            output,
            expected_source_type
        );
        if (!lowered_item.has_value()) {
            auto prefix =
                "lowering array-literal for statements requires an explicit Array<T, N> "
                "source type when element type cannot be inferred; add a typed local "
                "binding or cast the iterable with 'as Array<T, N>'";
            auto message = append_expression_lowering_failure(prefix, session.failures.expression);
            if (auto detail = generic_record_constructor_inference_failure_detail(
                    statement.expression.arguments[index],
                    context.lowering,
                    session.state
                )) {
                message = prefix + std::string {": "} + *detail;
            }
            diagnostics.error(
                statement.expression.arguments[index].line,
                message
            );
            return StatementFlow::failed;
        }

        session.state.immutable_bindings[statement.name] = LoweredExpression {
            .type = lowered_item->type,
            .value = lowered_item->value,
            .signedness = lowered_item->signedness,
        };
        if (auto source_type = source_type_name_for_llvm_type(lowered_item->type, context.lowering)) {
            session.state.source_type_names[statement.name] = std::move(*source_type);
        }
        bind_addressable_aggregate_value(
            statement.name,
            session.state.immutable_bindings.at(statement.name),
            session,
            output
        );

        auto loop_targets = LoopTargets {
            .break_target = block_plan.exit_block,
            .continue_target = next_for_iteration_target(block_plan, index),
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        };
        [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
        auto body_flow = lower_body();
        if (body_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (body_flow == StatementFlow::falls_through) {
            emit_llvm_branch(output, next_for_iteration_target(block_plan, index));
        }
    }

    emit_llvm_block_label(output, block_plan.exit_block);
    session.state.current_block = block_plan.exit_block;
    return StatementFlow::falls_through;
}

}  // namespace orison::lowering
