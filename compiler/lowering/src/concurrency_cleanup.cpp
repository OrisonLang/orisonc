#include "orison/lowering/concurrency_cleanup.hpp"

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
