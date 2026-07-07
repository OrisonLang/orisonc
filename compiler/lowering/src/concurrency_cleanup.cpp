#include "orison/lowering/concurrency_cleanup.hpp"

#include "orison/lowering/concurrency_runtime.hpp"

namespace orison::lowering {

auto emit_abandoned_concurrency_handle_cleanup(
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> void {
    auto destroy_call = concurrency_runtime_call(ConcurrencyRuntimeOperation::destroy_handle);
    for (auto const& binding_name : session.state.thread_binding_order) {
        auto binding = session.state.thread_bindings.find(binding_name);
        if (binding == session.state.thread_bindings.end() ||
            binding->second.handle_destroyed) {
            continue;
        }

        output << "  call " << destroy_call.return_type << " @" << destroy_call.symbol_name
               << "(ptr " << binding->second.handle << ")\n";
        binding->second.handle_destroyed = true;
    }
    for (auto const& binding_name : session.state.task_binding_order) {
        auto binding = session.state.task_bindings.find(binding_name);
        if (binding == session.state.task_bindings.end() ||
            binding->second.handle_destroyed) {
            continue;
        }

        output << "  call " << destroy_call.return_type << " @" << destroy_call.symbol_name
               << "(ptr " << binding->second.handle << ")\n";
        binding->second.handle_destroyed = true;
    }
}

}  // namespace orison::lowering
