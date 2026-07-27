#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
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
        if (context.options.test_only_enable_computed_dynamic_array_for_lowering) {
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
        auto const& computed_loop_exit_plan_for_gate =
            computed_production_emission_gate_plan.loop_exit_cleanup_plan;
        auto const& computed_loop_sequence_plan_for_gate =
            computed_loop_exit_plan_for_gate.loop_render_sequence_plan;
        auto const& computed_loop_continue_plan_for_gate =
            computed_loop_sequence_plan_for_gate.loop_continue_render_plan;
        auto const& computed_cleanup_sequence_plan_for_gate =
            computed_loop_continue_plan_for_gate.element_load_render_plan.element_address_render_plan
                .loop_control_render_plan.descriptor_render_plan.cleanup_sequence_plan;
        auto const computed_cleanup_transition_ready =
            computed_cleanup_sequence_plan_for_gate.cleanup_owner_name ==
                computed_loop_exit_plan_for_gate.loop_exit_cleanup_owner_name &&
            computed_cleanup_sequence_plan_for_gate.loop_entry_cleanup_owner_name ==
                computed_loop_exit_plan_for_gate.loop_entry_cleanup_owner_name &&
            !computed_cleanup_sequence_plan_for_gate.loop_entry_cleanup_operation_name.empty() &&
            !computed_loop_exit_plan_for_gate.cleanup_resumption_operation_name.empty();
        if (
            context.options.test_only_enable_computed_dynamic_array_for_lowering &&
            computed_production_emission_gate_plan.kind ==
                ComputedDynamicArrayIterableProductionEmissionGatePlanKind::production_emission_gate_planned &&
            computed_production_emission_gate_plan.ownership_ready &&
            computed_production_emission_gate_plan.loop_render_ready &&
            computed_production_emission_gate_plan.loop_cleanup_ownership_ready &&
            computed_production_emission_gate_plan.function_cleanup_resumption_ready &&
            computed_production_emission_gate_plan.exit_cleanup_ready &&
            computed_production_emission_gate_plan.production_sequence_render_planned &&
            computed_cleanup_transition_ready
        ) {
            auto const& loop_exit_plan = computed_production_emission_gate_plan.loop_exit_cleanup_plan;
            auto const& loop_sequence_plan = loop_exit_plan.loop_render_sequence_plan;
            auto const& loop_continue_plan = loop_sequence_plan.loop_continue_render_plan;
            auto const& element_load_plan = loop_continue_plan.element_load_render_plan;
            auto const& element_address_plan = element_load_plan.element_address_render_plan;
            auto const& loop_control_plan = element_address_plan.loop_control_render_plan;
            auto const& descriptor_plan = loop_control_plan.descriptor_render_plan;
            auto const& cleanup_sequence_plan = descriptor_plan.cleanup_sequence_plan;

            output << render_computed_dynamic_array_cleanup_state_handoff(
                ComputedDynamicArrayCleanupStateHandoff {
                    .kind = ComputedDynamicArrayCleanupStateHandoffKind::acquire,
                    .operation_name = cleanup_sequence_plan.loop_entry_cleanup_operation_name,
                    .source_owner_name = cleanup_sequence_plan.cleanup_owner_name,
                    .target_owner_name = cleanup_sequence_plan.loop_entry_cleanup_owner_name,
                    .cleanup_calls_enabled =
                        context.options.test_only_authorize_computed_dynamic_array_cleanup_calls,
                }
            );
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
            output << render_computed_dynamic_array_cleanup_state_handoff(
                ComputedDynamicArrayCleanupStateHandoff {
                    .kind = ComputedDynamicArrayCleanupStateHandoffKind::resume,
                    .operation_name = loop_exit_plan.cleanup_resumption_operation_name,
                    .source_owner_name = loop_exit_plan.loop_entry_cleanup_owner_name,
                    .target_owner_name = loop_exit_plan.loop_exit_cleanup_owner_name,
                    .cleanup_calls_enabled =
                        context.options.test_only_authorize_computed_dynamic_array_cleanup_calls,
                }
            );
            if (
                context.options.test_only_authorize_computed_dynamic_array_cleanup_calls &&
                context.options.test_only_insert_computed_dynamic_array_cleanup_calls
            ) {
                auto const element_size_bytes =
                    lowered_type_size_bytes(element_type->type, context.lowering);
                if (element_size_bytes.has_value()) {
                    auto cleanup_call_plan = DynamicArrayConstructionPlan {
                        .owner_name = cleanup_sequence_plan.cleanup_owner_name,
                        .source_type_name = std::string {*source_type_name},
                        .element_source_type_name = sequence->element_source_type_name,
                        .element_llvm_type = element_type->type,
                        .element_size_bytes = *element_size_bytes,
                        .operation = DynamicArrayRuntimeOperation::deallocate,
                    };
                    output << emit_dynamic_array_deallocation_call(
                        cleanup_call_plan,
                        descriptor_plan.data_pointer_name,
                        descriptor_plan.capacity_name
                    );
                }
            }
            session.state.current_block = loop_exit_plan.exit_block_name;
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
