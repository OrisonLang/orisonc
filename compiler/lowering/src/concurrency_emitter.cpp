#include "orison/lowering/concurrency_emitter.hpp"

#include "orison/lowering/concurrency_plan.hpp"
#include "orison/lowering/concurrency_runtime.hpp"
#include "orison/lowering/llvm_names.hpp"

#include <utility>

namespace orison::lowering {

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
    FunctionLoweringState& state,
    std::ostringstream& output
) -> LoweredExpression {
    auto join_call = concurrency_runtime_call(ConcurrencyRuntimeOperation::join_thread);
    output << "  call " << join_call.return_type << " @" << join_call.symbol_name
           << "(ptr " << binding.handle << ")\n";
    return emit_concurrency_result_load_and_destroy(binding, state, output);
}

auto emit_task_await_result(
    ConcurrencyBinding& binding,
    FunctionLoweringState& state,
    std::ostringstream& output
) -> LoweredExpression {
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
