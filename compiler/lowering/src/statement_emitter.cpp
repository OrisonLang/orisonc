#include "orison/lowering/statement_emitter.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/concurrency_emitter.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowered_value.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace orison::lowering {
namespace {

auto is_thread_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::thread;
}

auto is_task_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::task;
}

auto lower_thread_let_statement(
    syntax::StatementSyntax const& statement,
    ConcurrencyPlanKind expected_kind,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto const expression_name = expected_kind == ConcurrencyPlanKind::thread
        ? std::string_view {"thread"}
        : std::string_view {"task"};
    if (session.semantics == nullptr) {
        diagnostics.error(
            statement.line,
            "lowering " + std::string(expression_name) + " expressions requires semantic capture analysis"
        );
        return false;
    }

    auto plan = plan_concurrency_expression(
        statement.expression,
        session.enclosing_symbol_name,
        session.state.next_concurrency_ordinal++,
        context,
        session.state,
        *session.semantics
    );
    if (!plan.has_value() || plan->kind != expected_kind) {
        diagnostics.error(
            statement.line,
            "lowering does not yet support this " + std::string(expression_name) + " expression"
        );
        return false;
    }
    apply_drop_cleanup_authorization_options(plan->cleanup.drop_cleanup, context.options);

    auto thunk_definition = emit_concurrency_entry_thunk(
        *plan,
        statement.expression,
        context,
        session,
        diagnostics
    );
    if (!thunk_definition.has_value()) {
        return false;
    }
    auto cleanup_definition = emit_concurrency_cleanup_thunk(*plan);

    auto const binding_prefix = statement.name + "." + std::string(expression_name);
    auto environment_storage = next_llvm_local_value_name(
        binding_prefix + ".env",
        session.state.local_name_counts
    );
    output << "  " << environment_storage << " = alloca " << plan->environment_layout.llvm_type << "\n";
    auto capture_store_result = emit_concurrency_capture_environment_stores(
        *plan,
        environment_storage,
        session.state,
        output
    );
    if (capture_store_result == ConcurrencyCaptureStoreEmissionResult::unsupported_capture_type) {
        diagnostics.error(
            statement.line,
            "lowering does not yet support this " + std::string(expression_name) + " capture type"
        );
        return false;
    }
    if (capture_store_result == ConcurrencyCaptureStoreEmissionResult::missing_capture_source) {
        diagnostics.error(
            statement.line,
            "lowering does not yet support this " + std::string(expression_name) + " capture source"
        );
        return false;
    }

    auto result_storage = next_llvm_local_value_name(
        binding_prefix + ".result",
        session.state.local_name_counts
    );
    output << "  " << result_storage << " = alloca " << plan->result_storage.llvm_type << "\n";

    auto handle_name = next_llvm_local_value_name(statement.name, session.state.local_name_counts);
    emit_concurrency_spawn(*plan, handle_name, environment_storage, result_storage, output);

    auto const spawn_block_index = next_llvm_block_index(session.state.next_block_index);
    auto const spawn_failed_block = llvm_block_name(binding_prefix + ".spawn_failed", spawn_block_index);
    auto const spawn_ok_block = llvm_block_name(binding_prefix + ".spawn_ok", spawn_block_index);
    emit_concurrency_spawn_failure_check(
        handle_name,
        spawn_failed_block,
        spawn_ok_block,
        session.state,
        output
    );
    session.state.current_block = spawn_ok_block;

    register_concurrency_binding(
        expected_kind,
        statement.name,
        std::move(handle_name),
        std::move(result_storage),
        plan->result_type,
        session.state
    );
    queue_concurrency_function_definitions(
        *plan,
        std::move(*thunk_definition),
        std::move(cleanup_definition),
        session.state
    );
    return true;
}

struct LoweredAssignmentTarget {
    LoweredType type;
    std::string pointer;
};

auto lower_assignment_target(
    syntax::ExpressionSyntax const& target,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> std::optional<LoweredAssignmentTarget> {
    if (target.kind == syntax::ExpressionKind::name) {
        auto binding = session.state.mutable_bindings.find(target.text);
        if (binding == session.state.mutable_bindings.end()) {
            diagnostics.error(target.line, "lowering assignment target is not a mutable local");
            return std::nullopt;
        }
        return LoweredAssignmentTarget {
            .type = binding->second.type,
            .pointer = binding->second.storage,
        };
    }

    auto path = collect_aggregate_path(target);
    if (path.steps.empty() || path.base_expression == nullptr ||
        path.base_expression->kind != syntax::ExpressionKind::name) {
        diagnostics.error(
            target.line,
            "lowering only supports assignment to mutable local names and aggregate paths"
        );
        return std::nullopt;
    }

    auto const& base_expression = *path.base_expression;
    auto binding = session.state.mutable_bindings.find(base_expression.text);
    auto source_type = session.state.source_type_names.find(base_expression.text);
    if (source_type == session.state.source_type_names.end()) {
        diagnostics.error(
            target.line,
            "lowering aggregate assignment target type is unknown"
        );
        return std::nullopt;
    }

    auto current_source_type_name = source_type->second;
    auto current_pointer = std::string {};
    if (auto pointee_source_type = pointer_pointee_source_type_name(current_source_type_name)) {
        auto lowered_base = lower_expression(
            base_expression,
            "ptr",
            IntegerSignedness::not_integer,
            context,
            session,
            output
        );
        if (!lowered_base.has_value()) {
            auto detail = render_expression_lowering_failure(session.failures.expression);
            diagnostics.error(
                target.line,
                append_lowering_detail("lowering aggregate assignment target failed", detail)
            );
            return std::nullopt;
        }
        current_pointer = std::move(lowered_base->value);
        current_source_type_name = std::move(*pointee_source_type);
    } else {
        if (binding == session.state.mutable_bindings.end()) {
            diagnostics.error(target.line, "lowering assignment target is not a mutable local");
            return std::nullopt;
        }
        current_pointer = binding->second.storage;
    }

    auto cursor = initialize_aggregate_path_cursor(
        std::move(current_pointer),
        std::move(current_source_type_name),
        context.lowering
    );
    if (!cursor.has_value()) {
        diagnostics.error(target.line, "lowering aggregate assignment target type is unsupported");
        return std::nullopt;
    }

    for (auto const& step : path.steps) {
        if (step.kind == AggregatePathStepKind::member) {
            auto result = advance_aggregate_path_member_with_temporary(
                *cursor,
                step.field_name,
                context.lowering,
                session.state.next_temporary_index,
                output
            );
            if (result.error != AggregatePathError::none) {
                diagnostics.error(target.line, "lowering aggregate assignment member target is unsupported");
                return std::nullopt;
            }
            continue;
        }

        if (step.index_expression == nullptr) {
            diagnostics.error(target.line, "lowering aggregate assignment index target is unsupported");
            return std::nullopt;
        }

        auto lowered_index = lower_expression(
            *step.index_expression,
            "i64",
            IntegerSignedness::unsigned_integer,
            context,
            session,
            output
        );
        if (!lowered_index.has_value()) {
            auto detail = render_expression_lowering_failure(session.failures.expression);
            diagnostics.error(
                target.line,
                append_lowering_detail("lowering aggregate assignment index failed", detail)
            );
            return std::nullopt;
        }

        auto result = advance_aggregate_path_index_with_temporary(
            *cursor,
            lowered_index->value,
            context.lowering,
            session.state.next_temporary_index,
            output
        );
        if (result.error == AggregatePathError::unsupported_element_source_type) {
            diagnostics.error(target.line, "lowering aggregate assignment index source type is unsupported");
            return std::nullopt;
        }
        if (result.error != AggregatePathError::none) {
            diagnostics.error(
                target.line,
                result.error == AggregatePathError::expected_array
                    ? "lowering aggregate assignment index target is unsupported"
                    : "lowering aggregate assignment index target type is unsupported"
            );
            return std::nullopt;
        }
    }

    auto lowered_type = lowered_type_for_source_type_name(cursor->source_type_name, context.lowering);
    if (!lowered_type.has_value()) {
        diagnostics.error(target.line, "lowering aggregate assignment target type is unsupported");
        return std::nullopt;
    }

    return LoweredAssignmentTarget {
        .type = std::move(*lowered_type),
        .pointer = std::move(cursor->pointer),
    };
}

auto deferred_cleanup_block_for(
    syntax::StatementSyntax const& statement
) -> DeferredCleanupBlock {
    auto block = DeferredCleanupBlock {
        .line = statement.line,
    };
    block.statements.reserve(statement.nested_statements.size());
    for (auto const& nested_statement : statement.nested_statements) {
        block.statements.push_back(&nested_statement);
    }
    return block;
}

auto lower_prefix_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (statement.kind == syntax::StatementKind::let_binding) {
        return lower_let_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::var_binding) {
        return lower_var_statement(
            statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (statement.kind == syntax::StatementKind::assignment_statement) {
        return lower_assignment_statement(statement, context, session, diagnostics, output);
    }
    if (statement.kind == syntax::StatementKind::defer_statement) {
        return record_deferred_cleanup(statement, session, diagnostics);
    }
    return false;
}

auto emit_value_block_deferred_cleanup(
    DeferredCleanupScope const& defer_scope,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> bool {
    return emit_deferred_cleanup_to_depth(
        defer_scope.cleanup_depth(),
        context,
        session,
        diagnostics,
        output,
        lower_cleanup_block
    );
}

auto lower_value_statement_block(
    std::span<syntax::StatementSyntax const* const> statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }
    DeferredCleanupScope defer_scope(session.state);

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr ||
            !lower_prefix_statement(
                *statement,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                diagnostics,
                output
            )) {
            return std::nullopt;
        }
    }

    auto const* final_statement = statements.back();
    if (final_statement == nullptr) {
        return std::nullopt;
    }
    if (final_statement->kind == syntax::StatementKind::if_statement ||
        final_statement->kind == syntax::StatementKind::switch_statement) {
        auto lowered = lower_final_control_flow(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
        if (!lowered.has_value()) {
            return std::nullopt;
        }
        if (!emit_value_block_deferred_cleanup(
                defer_scope,
                context,
                session,
                diagnostics,
                output,
                lower_cleanup_block
            )) {
            return std::nullopt;
        }
        return lowered;
    }

    auto const* expression = value_expression_for(*final_statement);
    if (expression == nullptr) {
        return std::nullopt;
    }
    auto lowered = lower_expression(
        *expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        return std::nullopt;
    }
    if (!emit_value_block_deferred_cleanup(
            defer_scope,
            context,
            session,
            diagnostics,
            output,
            lower_cleanup_block
        )) {
        return std::nullopt;
    }
    return lowered;
}

auto lower_void_call_statement(
    syntax::StatementSyntax const& statement,
    std::string const& function_name,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (function.parameter_types.size() != statement.expression.arguments.size()) {
        session.failures.expression = ExpressionLoweringFailure {
            .reason = ExpressionLoweringFailureReason::call_arity_mismatch,
            .detail = function_name + " expects " +
                std::to_string(function.parameter_types.size()) + " arguments, got " +
                std::to_string(statement.expression.arguments.size()),
        };
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(statement.line, "lowering call statement failed: " + detail);
        return false;
    }

    auto arguments = lower_call_arguments(
        statement.expression,
        function,
        context,
        session,
        output
    );
    if (!arguments.has_value()) {
        if (session.failures.expression.reason == ExpressionLoweringFailureReason::none) {
            session.failures.expression = ExpressionLoweringFailure {
                .reason = ExpressionLoweringFailureReason::call_argument_failure,
                .detail = function_name,
            };
        }
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            append_lowering_detail("lowering call statement failed", detail)
        );
        return false;
    }

    emit_void_call(function, *arguments, output);
    return true;
}

struct ResolvedMemberCall {
    MemberCallReceiverInference receiver;
    LoweredMethodLookup method;
};

auto resolve_member_call(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state
) -> ResolvedMemberCall {
    auto receiver = infer_member_call_receiver(expression, context.lowering, state);
    if (receiver.result != MemberCallReceiverInferenceResult::found) {
        return {.receiver = std::move(receiver)};
    }

    auto method = find_lowered_method_signature(
        context.lowering,
        receiver.receiver_type_name,
        receiver.method_name
    );
    return ResolvedMemberCall {
        .receiver = std::move(receiver),
        .method = std::move(method),
    };
}

auto member_call_target_name(ResolvedMemberCall const& resolved) -> std::string {
    return resolved.receiver.receiver_type_name + "." + resolved.receiver.method_name;
}

auto diagnose_member_call_statement(
    syntax::StatementSyntax const& statement,
    ResolvedMemberCall const& resolved,
    diagnostics::DiagnosticBag& diagnostics
) -> LoweredFunctionSignature const* {
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::unsupported_shape) {
        diagnostics.error(statement.line, "lowering member call statement has unsupported receiver shape");
        return nullptr;
    }
    if (resolved.receiver.result == MemberCallReceiverInferenceResult::not_found) {
        diagnostics.error(statement.line, "lowering member call receiver type is unknown");
        return nullptr;
    }

    auto const target_name = member_call_target_name(resolved);
    if (resolved.method.result == LoweredMethodLookupResult::not_found) {
        diagnostics.error(
            statement.line,
            "lowering member call target is unknown: " + target_name
        );
        return nullptr;
    }
    if (resolved.method.result == LoweredMethodLookupResult::ambiguous) {
        diagnostics.error(
            statement.line,
            "lowering member call target is ambiguous: " + target_name
        );
        return nullptr;
    }

    if (resolved.method.method == nullptr ||
        !has_supported_function_signature_types(resolved.method.method->signature)) {
        diagnostics.error(
            statement.line,
            "lowering member call target is not lowerable: " + target_name
        );
        return nullptr;
    }

    return &resolved.method.method->signature;
}

auto lower_void_member_call_statement(
    syntax::StatementSyntax const& statement,
    syntax::ExpressionSyntax const& receiver_expression,
    std::string const& target_name,
    LoweredFunctionSignature const& function,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto const expected_argument_count = function.parameter_types.empty()
        ? std::size_t {0}
        : function.parameter_types.size() - 1;
    if (expected_argument_count != statement.expression.arguments.size()) {
        session.failures.expression = ExpressionLoweringFailure {
            .reason = ExpressionLoweringFailureReason::call_arity_mismatch,
            .detail = target_name + " expects " + std::to_string(expected_argument_count) +
                " arguments, got " + std::to_string(statement.expression.arguments.size()),
        };
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(statement.line, "lowering member call statement failed: " + detail);
        return false;
    }

    auto arguments = lower_member_call_arguments(
        receiver_expression,
        std::span<syntax::ExpressionSyntax const>(
            statement.expression.arguments.data(),
            statement.expression.arguments.size()
        ),
        function,
        context,
        session,
        output
    );
    if (!arguments.has_value()) {
        if (session.failures.expression.reason == ExpressionLoweringFailureReason::none) {
            session.failures.expression = ExpressionLoweringFailure {
                .reason = ExpressionLoweringFailureReason::call_argument_failure,
                .detail = target_name,
            };
        }
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            append_lowering_detail("lowering member call statement failed", detail)
        );
        return false;
    }

    emit_void_call(function, *arguments, output);
    return true;
}

}  // namespace

auto value_expression_for(
    syntax::StatementSyntax const& statement
) -> syntax::ExpressionSyntax const* {
    if (statement.kind == syntax::StatementKind::return_statement ||
        statement.kind == syntax::StatementKind::expression_statement) {
        return &statement.expression;
    }
    return nullptr;
}

auto lower_let_statement(
    syntax::StatementSyntax const& statement,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (is_thread_expression(statement.expression)) {
        return lower_thread_let_statement(
            statement,
            ConcurrencyPlanKind::thread,
            context,
            session,
            diagnostics,
            output
        );
    }
    if (is_task_expression(statement.expression)) {
        return lower_thread_let_statement(
            statement,
            ConcurrencyPlanKind::task,
            context,
            session,
            diagnostics,
            output
        );
    }

    auto type = LoweredType {
        .type = std::string(expected_llvm_type),
        .signedness = expected_signedness,
    };
    if (!statement.annotated_type.name.empty()) {
        auto annotated_type = lowered_type_for_source_type_name(
            render_source_type_name(statement.annotated_type),
            context.lowering
        );
        if (!annotated_type.has_value() || annotated_type->type == "void") {
            diagnostics.error(statement.line, "lowering does not yet support this let type");
            return false;
        }
        type = std::move(*annotated_type);
    } else if (auto inferred = infer_expression_type(statement.expression, context, session.state)) {
        type = std::move(*inferred);
    }

    auto lowered = lower_expression(
        statement.expression,
        type.type,
        type.signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            append_lowering_detail("lowering does not yet support this let initializer", detail)
        );
        return false;
    }
    auto const direct_binding = lowered->type == "ptr" || is_aggregate_llvm_type(lowered->type);
    auto local_name = direct_binding
        ? lowered->value
        : next_llvm_local_value_name(statement.name, session.state.local_name_counts);
    if (!direct_binding) {
        output << "  " << local_name << " = add " << lowered->type << " 0, " << lowered->value << "\n";
    }
    session.state.immutable_bindings[statement.name] = LoweredExpression {
        .type = lowered->type,
        .value = std::move(local_name),
        .signedness = lowered->signedness,
    };
    bind_addressable_aggregate_value(
        statement.name,
        session.state.immutable_bindings.at(statement.name),
        session,
        output
    );
    if (!statement.annotated_type.name.empty()) {
        session.state.source_type_names[statement.name] =
            render_source_type_name(statement.annotated_type);
    } else if (auto inferred_source_type =
                   source_type_name_for_initializer(
                       statement.expression,
                       context.lowering,
                       session.state,
                       lowered->type
                   )) {
        session.state.source_type_names[statement.name] = std::move(*inferred_source_type);
    }
    return true;
}

auto lower_var_statement(
    syntax::StatementSyntax const& statement,
    std::string_view fallback_llvm_type,
    IntegerSignedness fallback_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto type = LoweredType {
        .type = std::string(fallback_llvm_type),
        .signedness = fallback_signedness,
    };
    if (!statement.annotated_type.name.empty()) {
        auto annotated_type = lowered_type_for_source_type_name(
            render_source_type_name(statement.annotated_type),
            context.lowering
        );
        if (!annotated_type.has_value() || annotated_type->type == "void") {
            diagnostics.error(statement.line, "lowering does not yet support this var type");
            return false;
        }
        type = std::move(*annotated_type);
    } else if (auto inferred = infer_expression_type(statement.expression, context, session.state)) {
        type = std::move(*inferred);
    }

    auto lowered = lower_expression(
        statement.expression,
        type.type,
        type.signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            append_lowering_detail("lowering does not yet support this var initializer", detail)
        );
        return false;
    }

    auto storage = next_llvm_local_value_name(
        statement.name + ".addr",
        session.state.local_name_counts
    );
    output << "  " << storage << " = alloca " << type.type << "\n";
    output << "  store " << type.type << " " << lowered->value << ", ptr " << storage << "\n";
    session.state.mutable_bindings[statement.name] = MutableBinding {
        .type = std::move(type),
        .storage = std::move(storage),
    };
    if (!statement.annotated_type.name.empty()) {
        session.state.source_type_names[statement.name] =
            render_source_type_name(statement.annotated_type);
    } else if (auto inferred_source_type =
                   source_type_name_for_initializer(
                       statement.expression,
                       context.lowering,
                       session.state,
                       lowered->type
                   )) {
        session.state.source_type_names[statement.name] = std::move(*inferred_source_type);
    }
    return true;
}

auto lower_assignment_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    auto target = lower_assignment_target(
        statement.assignment_target,
        context,
        session,
        diagnostics,
        output
    );
    if (!target.has_value()) {
        return false;
    }

    auto lowered = lower_expression(
        statement.expression,
        target->type.type,
        target->type.signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            append_lowering_detail("lowering does not yet support this assignment value", detail)
        );
        return false;
    }

    output << "  store " << target->type.type << " " << lowered->value
           << ", ptr " << target->pointer << "\n";
    return true;
}

auto lower_call_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output
) -> bool {
    if (statement.kind != syntax::StatementKind::expression_statement ||
        statement.expression.kind != syntax::ExpressionKind::call) {
        diagnostics.error(statement.line, "lowering call statement requires a call expression");
        return false;
    }
    if (statement.expression.left != nullptr &&
        statement.expression.left->kind == syntax::ExpressionKind::null_safe_member_access) {
        diagnostics.error(
            statement.line,
            "lowering null-safe member call statements is not yet supported"
        );
        return false;
    }
    if (statement.expression.left != nullptr &&
        statement.expression.left->kind == syntax::ExpressionKind::member_access) {
        auto resolved = resolve_member_call(statement.expression, context, session.state);
        auto function = diagnose_member_call_statement(statement, resolved, diagnostics);
        if (function == nullptr) {
            return false;
        }

        auto const target_name = member_call_target_name(resolved);
        if (function->return_type == "void") {
            return lower_void_member_call_statement(
                statement,
                *statement.expression.left->left,
                target_name,
                *function,
                context,
                session,
                diagnostics,
                output
            );
        }

        auto lowered = lower_expression(
            statement.expression,
            function->return_type,
            function->return_signedness,
            context,
            session,
            output
        );
        if (!lowered.has_value()) {
            auto detail = render_expression_lowering_failure(session.failures.expression);
            diagnostics.error(
                statement.line,
                append_lowering_detail("lowering does not yet support this call statement", detail)
            );
            return false;
        }
        return true;
    }

    auto type = infer_expression_type(statement.expression, context, session.state);
    if (!type.has_value() || type->type.empty()) {
        diagnostics.error(statement.line, "lowering does not yet support this call statement result type");
        return false;
    }
    if (type->type == "void") {
        if (statement.expression.left != nullptr &&
            statement.expression.left->kind == syntax::ExpressionKind::name &&
            (statement.expression.left->text == "raw_write" ||
             statement.expression.left->text == "volatile_write")) {
            auto lowered = lower_expression(
                statement.expression,
                "void",
                IntegerSignedness::not_integer,
                context,
                session,
                output
            );
            if (!lowered.has_value()) {
                auto detail = render_expression_lowering_failure(session.failures.expression);
                diagnostics.error(
                    statement.line,
                    append_lowering_detail("lowering does not yet support this call statement", detail)
                );
                return false;
            }
            return true;
        }
        if (statement.expression.left == nullptr ||
            statement.expression.left->kind != syntax::ExpressionKind::name) {
            diagnostics.error(statement.line, "lowering call statement requires a direct function name");
            return false;
        }
        auto function = context.lowering.functions.find(statement.expression.left->text);
        if (function == context.lowering.functions.end()) {
            diagnostics.error(statement.line, "lowering call statement target is unknown");
            return false;
        }
        return lower_void_call_statement(
            statement,
            statement.expression.left->text,
            function->second,
            context,
            session,
            diagnostics,
            output
        );
    }
    auto lowered = lower_expression(
        statement.expression,
        type->type,
        type->signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        auto detail = render_expression_lowering_failure(session.failures.expression);
        diagnostics.error(
            statement.line,
            append_lowering_detail("lowering does not yet support this call statement", detail)
        );
        return false;
    }
    return true;
}

auto record_deferred_cleanup(
    syntax::StatementSyntax const& statement,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics
) -> bool {
    if (session.state.defer_cleanup_scopes.empty()) {
        diagnostics.error(statement.line, "lowering defer statements requires an active cleanup scope");
        return false;
    }

    session.state.defer_cleanup_scopes.back().blocks.push_back(deferred_cleanup_block_for(statement));
    return true;
}

auto lower_loop_control_statement(
    syntax::StatementSyntax const& statement,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> bool {
    if (statement.kind != syntax::StatementKind::break_statement &&
        statement.kind != syntax::StatementKind::continue_statement) {
        diagnostics.error(statement.line, "loop-control lowering requires break or continue");
        return false;
    }
    if (session.state.loop_targets.empty()) {
        diagnostics.error(statement.line, "loop-control lowering requires an active loop");
        return false;
    }

    auto const& targets = session.state.loop_targets.back();
    auto const& target = statement.kind == syntax::StatementKind::break_statement
        ? targets.break_target
        : targets.continue_target;
    if (!emit_deferred_cleanup_to_depth(
            targets.defer_cleanup_depth,
            context,
            session,
            diagnostics,
            output,
            lower_cleanup_block
        )) {
        return false;
    }
    emit_llvm_branch(output, target);
    return true;
}

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> std::optional<LoweredExpression> {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(&statement);
    }
    return lower_value_statement_block(
        statement_pointers,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_final_control_flow,
        lower_cleanup_block
    );
}

auto lower_value_statement_block(
    std::vector<syntax::StatementSyntax const*> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> std::optional<LoweredExpression> {
    if (statements.empty()) {
        return std::nullopt;
    }
    DeferredCleanupScope defer_scope(session.state);

    for (auto index = std::size_t {0}; index + 1 < statements.size(); ++index) {
        auto const* statement = statements[index];
        if (statement == nullptr ||
            !lower_prefix_statement(
                *statement,
                expected_llvm_type,
                expected_signedness,
                context,
                session,
                diagnostics,
                output
            )) {
            return std::nullopt;
        }
    }

    auto const* final_statement = statements.back();
    if (final_statement == nullptr) {
        return std::nullopt;
    }
    if (final_statement->kind == syntax::StatementKind::if_statement ||
        final_statement->kind == syntax::StatementKind::switch_statement) {
        auto lowered = lower_final_control_flow(
            *final_statement,
            expected_llvm_type,
            expected_signedness,
            context,
            session,
            diagnostics,
            output
        );
        if (!lowered.has_value()) {
            return std::nullopt;
        }
        if (!emit_value_block_deferred_cleanup(
                defer_scope,
                context,
                session,
                diagnostics,
                output,
                lower_cleanup_block
            )) {
            return std::nullopt;
        }
        return lowered;
    }

    auto const* expression = value_expression_for(*final_statement);
    if (expression == nullptr) {
        return std::nullopt;
    }
    auto lowered = lower_expression(
        *expression,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        output
    );
    if (!lowered.has_value()) {
        return std::nullopt;
    }
    if (!emit_value_block_deferred_cleanup(
            defer_scope,
            context,
            session,
            diagnostics,
            output,
            lower_cleanup_block
        )) {
        return std::nullopt;
    }
    return lowered;
}

auto lower_value_statement_block(
    std::vector<std::unique_ptr<syntax::StatementSyntax>> const& statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> std::optional<LoweredExpression> {
    auto statement_pointers = std::vector<syntax::StatementSyntax const*> {};
    statement_pointers.reserve(statements.size());
    for (auto const& statement : statements) {
        statement_pointers.push_back(statement.get());
    }
    return lower_value_statement_block(
        statement_pointers,
        expected_llvm_type,
        expected_signedness,
        context,
        session,
        diagnostics,
        output,
        lower_final_control_flow,
        lower_cleanup_block
    );
}

}  // namespace orison::lowering
