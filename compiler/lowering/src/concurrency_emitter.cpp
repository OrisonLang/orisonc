#include "orison/lowering/concurrency_emitter.hpp"

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/expression_emitter.hpp"
#include "orison/lowering/llvm_cfg.hpp"
#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/lowering_diagnostics.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <utility>

namespace orison::lowering {

namespace {

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

void record_expression_failure(
    LoweringFailures& failures,
    ExpressionLoweringFailureReason reason,
    std::string detail
) {
    if (failures.expression.reason == ExpressionLoweringFailureReason::none) {
        failures.expression = ExpressionLoweringFailure {
            .reason = reason,
            .detail = std::move(detail),
        };
    }
}

}  // namespace

auto emit_concurrency_handle_destroy(
    ConcurrencyBinding& binding,
    std::ostringstream& output
) -> void {
    auto destroy_call = concurrency_runtime_call(ConcurrencyRuntimeOperation::destroy_handle);
    output << "  call " << destroy_call.return_type << " @" << destroy_call.symbol_name
           << "(ptr " << binding.handle << ")\n";
    binding.handle_destroyed = true;
}

auto emit_concurrency_result_load_and_destroy(
    ConcurrencyBinding& binding,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> LoweredExpression {
    auto result_name = next_llvm_temporary_name(state.next_temporary_index);
    output << "  " << result_name << " = load " << binding.result_type.type
           << ", ptr " << binding.result_storage << "\n";
    emit_concurrency_handle_destroy(binding, output);
    return LoweredExpression {
        .type = binding.result_type.type,
        .value = std::move(result_name),
        .signedness = binding.result_type.signedness,
    };
}

auto emit_thread_join_result(
    ConcurrencyBinding& binding,
    std::string_view expected_llvm_type,
    FunctionLoweringState& state,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (binding.result_type.type != expected_llvm_type) {
        record_expression_failure(
            failures,
            ExpressionLoweringFailureReason::call_return_type_mismatch,
            "thread join returns " + binding.result_type.type +
                ", expected " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }
    auto join_call = concurrency_runtime_call(ConcurrencyRuntimeOperation::join_thread);
    output << "  call " << join_call.return_type << " @" << join_call.symbol_name
           << "(ptr " << binding.handle << ")\n";
    return emit_concurrency_result_load_and_destroy(binding, state, output);
}

auto emit_task_await_result(
    ConcurrencyBinding& binding,
    std::string_view expected_llvm_type,
    FunctionLoweringState& state,
    LoweringFailures& failures,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (binding.result_type.type != expected_llvm_type) {
        record_expression_failure(
            failures,
            ExpressionLoweringFailureReason::call_return_type_mismatch,
            "task await returns " + binding.result_type.type +
                ", expected " + std::string(expected_llvm_type)
        );
        return std::nullopt;
    }
    auto await_call = concurrency_runtime_call(ConcurrencyRuntimeOperation::await_task);
    output << "  call " << await_call.return_type << " @" << await_call.symbol_name
           << "(ptr " << binding.handle << ")\n";
    return emit_concurrency_result_load_and_destroy(binding, state, output);
}

auto emit_concurrency_spawn_failed(
    std::ostringstream& output
) -> void {
    auto spawn_failed_call = concurrency_runtime_call(ConcurrencyRuntimeOperation::spawn_failed);
    output << "  call " << spawn_failed_call.return_type << " @"
           << spawn_failed_call.symbol_name << "()\n";
}

auto emit_concurrency_spawn_failure_check(
    std::string_view handle_name,
    std::string_view spawn_failed_block,
    std::string_view spawn_ok_block,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> void {
    auto const spawn_failure_check = next_llvm_temporary_name(state.next_temporary_index);
    output << "  " << spawn_failure_check << " = icmp eq ptr " << handle_name << ", null\n";
    emit_llvm_conditional_branch(output, spawn_failure_check, spawn_failed_block, spawn_ok_block);
    emit_llvm_block_label(output, spawn_failed_block);
    emit_concurrency_spawn_failed(output);
    emit_llvm_unreachable(output);
    emit_llvm_block_label(output, spawn_ok_block);
}

auto emit_concurrency_spawn(
    ConcurrencyExpressionPlan const& plan,
    std::string_view handle_name,
    std::string_view environment_storage,
    std::string_view result_storage,
    std::ostringstream& output
) -> void {
    auto runtime_call = concurrency_runtime_call(plan.spawn_operation);
    auto cleanup_pointer = plan.captures.empty()
        ? std::string {"null"}
        : "@" + plan.cleanup_symbol_name;
    output << "  " << handle_name << " = call " << runtime_call.return_type
           << " @" << runtime_call.symbol_name
           << "(ptr @" << plan.thunk_symbol_name
           << ", ptr " << environment_storage
           << ", ptr " << result_storage
           << ", i64 " << plan.result_storage.size_bytes
           << ", ptr " << cleanup_pointer << ")\n";
}

auto emit_concurrency_capture_environment_stores(
    ConcurrencyExpressionPlan const& plan,
    std::string_view environment_storage,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> ConcurrencyCaptureStoreEmissionResult {
    for (auto const& capture : plan.captures) {
        if (capture.llvm_type.empty()) {
            return ConcurrencyCaptureStoreEmissionResult::unsupported_capture_type;
        }
        auto captured_value = state.immutable_bindings.find(capture.name);
        if (captured_value == state.immutable_bindings.end()) {
            return ConcurrencyCaptureStoreEmissionResult::missing_capture_source;
        }
        auto field_pointer = next_llvm_temporary_name(state.next_temporary_index);
        output << "  " << field_pointer << " = getelementptr " << plan.environment_layout.llvm_type
               << ", ptr " << environment_storage << ", i32 0, i32 " << capture.field_index << "\n";
        output << "  store " << capture.llvm_type << " " << captured_value->second.value
               << ", ptr " << field_pointer << "\n";
    }
    return ConcurrencyCaptureStoreEmissionResult::emitted;
}

auto register_concurrency_binding(
    ConcurrencyPlanKind kind,
    std::string const& binding_name,
    std::string handle_name,
    std::string result_storage,
    LoweredType result_type,
    FunctionLoweringState& state
) -> void {
    state.immutable_bindings[binding_name] = LoweredExpression {
        .type = std::string(concurrency_handle_llvm_type()),
        .value = handle_name,
        .signedness = IntegerSignedness::not_integer,
    };
    auto binding = ConcurrencyBinding {
        .handle = std::move(handle_name),
        .result_storage = std::move(result_storage),
        .result_type = std::move(result_type),
    };
    if (kind == ConcurrencyPlanKind::thread) {
        state.thread_bindings[binding_name] = std::move(binding);
        state.thread_binding_order.push_back(binding_name);
        return;
    }
    state.task_bindings[binding_name] = std::move(binding);
    state.task_binding_order.push_back(binding_name);
}

auto queue_concurrency_function_definitions(
    ConcurrencyExpressionPlan const& plan,
    std::string entry_thunk_definition,
    std::string cleanup_definition,
    FunctionLoweringState& state
) -> void {
    state.pending_function_definitions.push_back(std::move(entry_thunk_definition));
    if (!plan.captures.empty()) {
        state.pending_function_definitions.push_back(std::move(cleanup_definition));
    }
}

auto emit_concurrency_entry_thunk(
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

auto emit_concurrency_cleanup_thunk(
    ConcurrencyExpressionPlan const& plan
) -> std::string {
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

auto emit_abandoned_concurrency_handle_cleanup(
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> void {
    for (auto const& binding_name : session.state.thread_binding_order) {
        auto binding = session.state.thread_bindings.find(binding_name);
        if (binding == session.state.thread_bindings.end() ||
            binding->second.handle_destroyed) {
            continue;
        }

        emit_concurrency_handle_destroy(binding->second, output);
    }
    for (auto const& binding_name : session.state.task_binding_order) {
        auto binding = session.state.task_bindings.find(binding_name);
        if (binding == session.state.task_bindings.end() ||
            binding->second.handle_destroyed) {
            continue;
        }

        emit_concurrency_handle_destroy(binding->second, output);
    }
}

}  // namespace orison::lowering
