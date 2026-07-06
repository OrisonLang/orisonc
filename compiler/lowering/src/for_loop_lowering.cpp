#include "orison/lowering/for_loop_lowering.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace orison::lowering {

auto lower_fixed_array_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    ForLoopBodyLowerer lower_body
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
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.expression.line,
            "lowering for statements currently requires a fixed-size array iterable" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return StatementFlow::failed;
    }

    auto const block_index = next_llvm_block_index(session.state.next_block_index);
    auto const exit_block = llvm_block_name("for.exit", block_index);
    auto iteration_blocks = std::vector<std::string> {};
    iteration_blocks.reserve(array_type->length);
    for (auto index = std::size_t {0}; index < array_type->length; ++index) {
        iteration_blocks.push_back(llvm_block_name("for.iteration", block_index, index));
    }

    if (iteration_blocks.empty()) {
        emit_llvm_branch(output, exit_block);
        emit_llvm_block_label(output, exit_block);
        session.state.current_block = exit_block;
        return StatementFlow::falls_through;
    }

    auto loop_scope = BranchBindingScope(session.state);
    emit_llvm_branch(output, iteration_blocks.front());

    for (auto index = std::size_t {0}; index < array_type->length; ++index) {
        loop_scope.reset();
        emit_llvm_block_label(output, iteration_blocks[index]);
        session.state.current_block = iteration_blocks[index];

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
            .break_target = exit_block,
            .continue_target = index + 1 < array_type->length ? iteration_blocks[index + 1] : exit_block,
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        };
        [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
        auto body_flow = lower_body();
        if (body_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (body_flow == StatementFlow::falls_through) {
            emit_llvm_branch(output, index + 1 < array_type->length ? iteration_blocks[index + 1]
                                                                    : exit_block);
        }
    }

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return StatementFlow::falls_through;
}

auto lower_array_literal_for_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    ForLoopElementTypeInferer infer_element_type,
    ForLoopBodyLowerer lower_body
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

    auto const block_index = next_llvm_block_index(session.state.next_block_index);
    auto const exit_block = llvm_block_name("for.exit", block_index);
    auto iteration_blocks = std::vector<std::string> {};
    iteration_blocks.reserve(statement.expression.arguments.size());
    for (auto index = std::size_t {0}; index < statement.expression.arguments.size(); ++index) {
        iteration_blocks.push_back(llvm_block_name("for.iteration", block_index, index));
    }

    if (iteration_blocks.empty()) {
        emit_llvm_branch(output, exit_block);
        emit_llvm_block_label(output, exit_block);
        session.state.current_block = exit_block;
        return StatementFlow::falls_through;
    }

    auto loop_scope = BranchBindingScope(session.state);
    emit_llvm_branch(output, iteration_blocks.front());

    for (auto index = std::size_t {0}; index < statement.expression.arguments.size(); ++index) {
        loop_scope.reset();
        emit_llvm_block_label(output, iteration_blocks[index]);
        session.state.current_block = iteration_blocks[index];

        auto lowered_item = lower_expression(
            statement.expression.arguments[index],
            element_type->type,
            element_type->signedness,
            context,
            session,
            output
        );
        if (!lowered_item.has_value()) {
            auto detail = render_expression_lowering_failure(session.failures.expression);
            diagnostics.error(
                statement.expression.arguments[index].line,
                "lowering does not yet support this for iterable element" +
                    (detail.empty() ? std::string {} : ": " + detail)
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
            .break_target = exit_block,
            .continue_target = index + 1 < statement.expression.arguments.size()
                ? iteration_blocks[index + 1]
                : exit_block,
            .defer_cleanup_depth = session.state.defer_cleanup_scopes.size(),
        };
        [[maybe_unused]] auto target_scope = LoopTargetScope {session.state, std::move(loop_targets)};
        auto body_flow = lower_body();
        if (body_flow == StatementFlow::failed) {
            return StatementFlow::failed;
        }
        if (body_flow == StatementFlow::falls_through) {
            emit_llvm_branch(
                output,
                index + 1 < statement.expression.arguments.size()
                    ? iteration_blocks[index + 1]
                    : exit_block
            );
        }
    }

    emit_llvm_block_label(output, exit_block);
    session.state.current_block = exit_block;
    return StatementFlow::falls_through;
}

}  // namespace orison::lowering
