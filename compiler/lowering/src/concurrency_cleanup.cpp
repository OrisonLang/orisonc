#include "orison/lowering/concurrency_cleanup.hpp"

#include "orison/lowering/concurrency_runtime.hpp"

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
