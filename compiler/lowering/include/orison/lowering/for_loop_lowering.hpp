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
auto lower_dynamic_array_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    LowerBody&& lower_body
) -> StatementFlow {
    if (!context.options.enable_dynamic_array_for_lowering ||
        statement.expression.kind != syntax::ExpressionKind::name) {
        diagnostics.error(
            statement.line,
            "lowering dynamic-sequence for statements currently requires a named iterable"
        );
        return StatementFlow::failed;
    }

    auto const& owner_name = statement.expression.text;
    auto source_type = session.state.source_type_names.find(owner_name);
    if (source_type == session.state.source_type_names.end()) {
        diagnostics.error(
            statement.line,
            "lowering dynamic-sequence for statements currently requires a named iterable"
        );
        return StatementFlow::failed;
    }

    auto sequence = dynamic_sequence_source_type(source_type->second);
    if (!sequence.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering dynamic-sequence for statements currently requires a DynamicArray or View iterable"
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

    auto storage = aggregate_storage_for_name(owner_name, session.state);
    if (!storage.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering dynamic-sequence for statements currently requires descriptor storage"
        );
        return StatementFlow::failed;
    }

    auto element_llvm_type = llvm_type_for_source_type_name(sequence->element_source_type_name, context.lowering);
    if (!element_llvm_type.has_value()) {
        diagnostics.error(
            statement.line,
            "lowering dynamic-sequence for statements currently requires descriptor metadata"
        );
        return StatementFlow::failed;
    }

    auto prefix = "%" + owner_name + ".dynamic_array_for" +
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
    } else {
        output << "  " << prefix << ".descriptor = load " << view_descriptor_llvm_type();
        output << ", ptr " << *storage << "\n";
        output << emit_view_descriptor_field_projection(prefix + ".data", prefix + ".descriptor", 0);
        output << emit_view_descriptor_field_projection(prefix + ".length", prefix + ".descriptor", 1);
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
