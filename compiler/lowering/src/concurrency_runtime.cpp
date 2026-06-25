#include "orison/lowering/concurrency_runtime.hpp"

namespace orison::lowering {

auto concurrency_handle_llvm_type() -> std::string_view {
    return "ptr";
}

auto concurrency_runtime_call(
    ConcurrencyRuntimeOperation operation
) -> ConcurrencyRuntimeCall {
    switch (operation) {
    case ConcurrencyRuntimeOperation::spawn_thread:
        return ConcurrencyRuntimeCall {
            .symbol_name = "__orison_thread_spawn",
            .return_type = concurrency_handle_llvm_type(),
            .parameter_types = {"ptr", "ptr", "ptr", "i64", "ptr"},
        };
    case ConcurrencyRuntimeOperation::join_thread:
        return ConcurrencyRuntimeCall {
            .symbol_name = "__orison_thread_join",
            .return_type = "void",
            .parameter_types = {concurrency_handle_llvm_type()},
        };
    case ConcurrencyRuntimeOperation::spawn_task:
        return ConcurrencyRuntimeCall {
            .symbol_name = "__orison_task_spawn",
            .return_type = concurrency_handle_llvm_type(),
            .parameter_types = {"ptr", "ptr", "ptr", "i64", "ptr"},
        };
    case ConcurrencyRuntimeOperation::await_task:
        return ConcurrencyRuntimeCall {
            .symbol_name = "__orison_task_await",
            .return_type = "void",
            .parameter_types = {concurrency_handle_llvm_type()},
        };
    case ConcurrencyRuntimeOperation::destroy_handle:
        return ConcurrencyRuntimeCall {
            .symbol_name = "__orison_concurrency_handle_destroy",
            .return_type = "void",
            .parameter_types = {concurrency_handle_llvm_type()},
        };
    }
    return {};
}

}  // namespace orison::lowering
