#include "orison/lowering/statement_emitter.hpp"

#include "orison/lowering/addressable_binding.hpp"
#include "orison/lowering/aggregate_path.hpp"
#include "orison/lowering/call_emitter.hpp"
#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/drop_metadata.hpp"
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

auto source_type_name_for_initializer(
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringState const& state,
    std::string_view lowered_llvm_type
) -> std::optional<std::string> {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto source_type = state.source_type_names.find(expression.text);
        if (source_type != state.source_type_names.end()) {
            return source_type->second;
        }
    }

    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name &&
        context.lowering.records.contains(expression.left->text)) {
        return expression.left->text;
    }

    if (auto source_type = source_type_name_for_llvm_type(lowered_llvm_type, context.lowering)) {
        return source_type;
    }

    return std::nullopt;
}

auto is_thread_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::thread;
}

auto is_task_expression(syntax::ExpressionSyntax const& expression) -> bool {
    return expression.kind == syntax::ExpressionKind::task;
}

auto thread_body_result_expression(
    syntax::ExpressionSyntax const& expression
) -> syntax::ExpressionSyntax const* {
    if (expression.nested_statements.empty() ||
        expression.nested_statements.back() == nullptr) {
        return nullptr;
    }

    auto const& final_statement = *expression.nested_statements.back();
    if (final_statement.kind == syntax::StatementKind::expression_statement ||
        final_statement.kind == syntax::StatementKind::return_statement) {
        return &final_statement.expression;
    }
    return nullptr;
}

auto emit_thread_entry_thunk(
    ConcurrencyExpressionPlan const& plan,
    syntax::ExpressionSyntax const& expression,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& parent_session,
    diagnostics::DiagnosticBag& diagnostics
) -> std::optional<std::string> {
    auto const expression_name = plan.kind == ConcurrencyPlanKind::thread
        ? std::string_view {"thread"}
        : std::string_view {"task"};
    auto const* result_expression = thread_body_result_expression(expression);
    if (result_expression == nullptr) {
        diagnostics.error(
            expression.line,
            "lowering does not yet support this " + std::string(expression_name) + " body"
        );
        return std::nullopt;
    }

    auto output = std::ostringstream {};
    output << "define private void @" << plan.thunk_symbol_name
           << "(ptr %environment, ptr %result_storage) {\n";
    output << "entry:\n";

    auto thunk_state = FunctionLoweringState {};
    auto thunk_failures = LoweringFailures {};
    auto thunk_session = FunctionLoweringSession {
        .state = thunk_state,
        .failures = thunk_failures,
        .semantics = parent_session.semantics,
        .enclosing_symbol_name = plan.thunk_symbol_name,
    };

    for (auto const& capture : plan.captures) {
        auto field_pointer = next_llvm_temporary_name(thunk_state.next_temporary_index);
        output << "  " << field_pointer << " = getelementptr "
               << plan.environment_layout.llvm_type
               << ", ptr %environment, i32 0, i32 " << capture.field_index << "\n";
        auto loaded_capture = next_llvm_temporary_name(thunk_state.next_temporary_index);
        output << "  " << loaded_capture << " = load " << capture.llvm_type
               << ", ptr " << field_pointer << "\n";
        auto capture_type = lowered_type_for_source_type_name(capture.source_type_name, context.lowering);
        thunk_state.immutable_bindings[capture.name] = LoweredExpression {
            .type = capture.llvm_type,
            .value = std::move(loaded_capture),
            .signedness = capture_type.has_value()
                ? capture_type->signedness
                : IntegerSignedness::not_integer,
        };
        thunk_state.source_type_names[capture.name] = capture.source_type_name;
    }

    auto lowered_result = lower_expression(
        *result_expression,
        plan.result_type.type,
        plan.result_type.signedness,
        context,
        thunk_session,
        output
    );
    if (!lowered_result.has_value()) {
        auto detail = render_expression_lowering_failure(thunk_failures.expression);
        diagnostics.error(
            result_expression->line,
            "lowering does not yet support this " + std::string(expression_name) + " body result" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return std::nullopt;
    }

    output << "  store " << plan.result_type.type << " " << lowered_result->value
           << ", ptr %result_storage\n";
    output << "  ret void\n";
    output << "}\n";
    return output.str();
}

auto emit_thread_cleanup_thunk(ConcurrencyExpressionPlan const& plan) -> std::string {
    auto output = std::ostringstream {};
    output << "define private void @" << plan.cleanup_symbol_name << "(ptr %environment) {\n";
    output << "entry:\n";
    for (auto index = std::size_t {0}; index < plan.cleanup.drop_candidates.size(); ++index) {
        auto const& candidate = plan.cleanup.drop_candidates[index];
        auto const& action = plan.cleanup.drop_cleanup.actions[index];
        auto field_pointer = "%cleanup.field." + std::to_string(candidate.field_index);
        output << "  " << field_pointer << " = getelementptr " << plan.environment_layout.llvm_type
               << ", ptr %environment, i32 0, i32 " << candidate.field_index << "\n";
        output << "  ; cleanup candidate " << action.capture_name << ": " << action.source_type_name
               << " field " << action.field_index << " drop " << action.symbol_name << "\n";
        if (drop_calls_enabled(plan.cleanup.drop_cleanup)) {
            output << "  call void @" << action.symbol_name << "(ptr " << field_pointer << ")\n";
        }
    }
    output << "  ret void\n";
    output << "}\n";
    return output.str();
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
    if (!context.options.test_only_declared_drop_source_type_allowlist.empty()) {
        auto drop_declarations = declared_drop_declarations_for_allowed_source_types(
            plan->cleanup.drop_cleanup.actions,
            context.options.test_only_declared_drop_source_type_allowlist
        );
        authorize_drop_cleanup_calls_for_declared_abi(plan->cleanup.drop_cleanup, drop_declarations);
    } else if (!context.options.semantic_drop_lowering_authorizations.empty()) {
        auto drop_declarations = declared_drop_declarations_for_authorized_semantic_drops(
            context.options.semantic_drop_lowering_authorizations
        );
        authorize_drop_cleanup_calls_for_declared_abi(plan->cleanup.drop_cleanup, drop_declarations);
    }

    auto thunk_definition = emit_thread_entry_thunk(
        *plan,
        statement.expression,
        context,
        session,
        diagnostics
    );
    if (!thunk_definition.has_value()) {
        return false;
    }
    auto cleanup_definition = emit_thread_cleanup_thunk(*plan);

    auto const binding_prefix = statement.name + "." + std::string(expression_name);
    auto environment_storage = next_llvm_local_value_name(
        binding_prefix + ".env",
        session.state.local_name_counts
    );
    output << "  " << environment_storage << " = alloca " << plan->environment_layout.llvm_type << "\n";
    for (auto const& capture : plan->captures) {
        if (capture.llvm_type.empty()) {
            diagnostics.error(
                statement.line,
                "lowering does not yet support this " + std::string(expression_name) + " capture type"
            );
            return false;
        }
        auto captured_value = session.state.immutable_bindings.find(capture.name);
        if (captured_value == session.state.immutable_bindings.end()) {
            diagnostics.error(
                statement.line,
                "lowering does not yet support this " + std::string(expression_name) + " capture source"
            );
            return false;
        }
        auto field_pointer = next_llvm_temporary_name(session.state.next_temporary_index);
        output << "  " << field_pointer << " = getelementptr " << plan->environment_layout.llvm_type
               << ", ptr " << environment_storage << ", i32 0, i32 " << capture.field_index << "\n";
        output << "  store " << capture.llvm_type << " " << captured_value->second.value
               << ", ptr " << field_pointer << "\n";
    }

    auto result_storage = next_llvm_local_value_name(
        binding_prefix + ".result",
        session.state.local_name_counts
    );
    output << "  " << result_storage << " = alloca " << plan->result_storage.llvm_type << "\n";

    auto handle_name = next_llvm_local_value_name(statement.name, session.state.local_name_counts);
    auto runtime_call = concurrency_runtime_call(plan->spawn_operation);
    auto cleanup_pointer = plan->captures.empty()
        ? std::string {"null"}
        : "@" + plan->cleanup_symbol_name;
    output << "  " << handle_name << " = call " << runtime_call.return_type
           << " @" << runtime_call.symbol_name
           << "(ptr @" << plan->thunk_symbol_name
           << ", ptr " << environment_storage
           << ", ptr " << result_storage
           << ", i64 " << plan->result_storage.size_bytes
           << ", ptr " << cleanup_pointer << ")\n";

    auto const spawn_failure_check = next_llvm_temporary_name(session.state.next_temporary_index);
    auto const spawn_block_index = next_llvm_block_index(session.state.next_block_index);
    auto const spawn_failed_block = llvm_block_name(binding_prefix + ".spawn_failed", spawn_block_index);
    auto const spawn_ok_block = llvm_block_name(binding_prefix + ".spawn_ok", spawn_block_index);
    output << "  " << spawn_failure_check << " = icmp eq ptr " << handle_name << ", null\n";
    emit_llvm_conditional_branch(output, spawn_failure_check, spawn_failed_block, spawn_ok_block);
    emit_llvm_block_label(output, spawn_failed_block);
    auto spawn_failed_call = concurrency_runtime_call(ConcurrencyRuntimeOperation::spawn_failed);
    output << "  call " << spawn_failed_call.return_type << " @" << spawn_failed_call.symbol_name << "()\n";
    emit_llvm_unreachable(output);
    emit_llvm_block_label(output, spawn_ok_block);
    session.state.current_block = spawn_ok_block;

    session.state.immutable_bindings[statement.name] = LoweredExpression {
        .type = std::string(concurrency_handle_llvm_type()),
        .value = handle_name,
        .signedness = IntegerSignedness::not_integer,
    };
    auto binding = ConcurrencyBinding {
        .handle = std::move(handle_name),
        .result_storage = std::move(result_storage),
        .result_type = plan->result_type,
    };
    if (expected_kind == ConcurrencyPlanKind::thread) {
        session.state.thread_bindings[statement.name] = std::move(binding);
        session.state.thread_binding_order.push_back(statement.name);
    } else {
        session.state.task_bindings[statement.name] = std::move(binding);
        session.state.task_binding_order.push_back(statement.name);
    }
    session.state.pending_function_definitions.push_back(std::move(*thunk_definition));
    if (!plan->captures.empty()) {
        session.state.pending_function_definitions.push_back(std::move(cleanup_definition));
    }
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
                "lowering aggregate assignment target failed" +
                    (detail.empty() ? std::string {} : ": " + detail)
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
                "lowering aggregate assignment index failed" +
                    (detail.empty() ? std::string {} : ": " + detail)
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

auto lower_value_statement_block(
    std::span<syntax::StatementSyntax const* const> statements,
    std::string_view expected_llvm_type,
    IntegerSignedness expected_signedness,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    FinalControlFlowLowerer lower_final_control_flow
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
        if (!emit_deferred_cleanup_to_depth(
                defer_scope.cleanup_depth(),
                context,
                session,
                diagnostics,
                output
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
    if (!emit_deferred_cleanup_to_depth(
            defer_scope.cleanup_depth(),
            context,
            session,
            diagnostics,
            output
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
            "lowering call statement failed" +
                (detail.empty() ? std::string {} : ": " + detail)
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
            "lowering member call statement failed" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }

    emit_void_call(function, *arguments, output);
    return true;
}

}  // namespace

auto emit_concurrency_cleanup_thunk(
    ConcurrencyExpressionPlan const& plan
) -> std::string {
    return emit_thread_cleanup_thunk(plan);
}

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
            "lowering does not yet support this let initializer" +
                (detail.empty() ? std::string {} : ": " + detail)
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
                   source_type_name_for_initializer(statement.expression, context, session.state, lowered->type)) {
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
            "lowering does not yet support this var initializer" +
                (detail.empty() ? std::string {} : ": " + detail)
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
                   source_type_name_for_initializer(statement.expression, context, session.state, lowered->type)) {
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
            "lowering does not yet support this assignment value" +
                (detail.empty() ? std::string {} : ": " + detail)
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
                "lowering does not yet support this call statement" +
                    (detail.empty() ? std::string {} : ": " + detail)
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
                    "lowering does not yet support this call statement" +
                        (detail.empty() ? std::string {} : ": " + detail)
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
            "lowering does not yet support this call statement" +
                (detail.empty() ? std::string {} : ": " + detail)
        );
        return false;
    }
    return true;
}

DeferredCleanupScope::DeferredCleanupScope(FunctionLoweringState& state)
    : state_(state),
      cleanup_depth_(state.defer_cleanup_scopes.size()) {
    state_.defer_cleanup_scopes.emplace_back();
}

DeferredCleanupScope::~DeferredCleanupScope() noexcept {
    state_.defer_cleanup_scopes.pop_back();
}

auto DeferredCleanupScope::cleanup_depth() const -> std::size_t {
    return cleanup_depth_;
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
    std::ostringstream& output
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
    if (!emit_deferred_cleanup_to_depth(targets.defer_cleanup_depth, context, session, diagnostics, output)) {
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
    FinalControlFlowLowerer lower_final_control_flow
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
        lower_final_control_flow
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
    FinalControlFlowLowerer lower_final_control_flow
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
        if (!emit_deferred_cleanup_to_depth(
                defer_scope.cleanup_depth(),
                context,
                session,
                diagnostics,
                output
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
    if (!emit_deferred_cleanup_to_depth(
            defer_scope.cleanup_depth(),
            context,
            session,
            diagnostics,
            output
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
    FinalControlFlowLowerer lower_final_control_flow
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
        lower_final_control_flow
    );
}

}  // namespace orison::lowering
