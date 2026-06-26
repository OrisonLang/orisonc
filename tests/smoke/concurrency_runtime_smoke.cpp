#include "orison/lowering/concurrency_runtime.hpp"

#include <cassert>
#include <string_view>
#include <vector>

int main() {
    using orison::lowering::ConcurrencyRuntimeOperation;

    assert(orison::lowering::concurrency_handle_llvm_type() == "ptr");

    auto thread_spawn = orison::lowering::concurrency_runtime_call(
        ConcurrencyRuntimeOperation::spawn_thread
    );
    assert(thread_spawn.symbol_name == "__orison_thread_spawn");
    assert(thread_spawn.return_type == "ptr");
    assert(
        thread_spawn.parameter_types ==
        std::vector<std::string_view>({"ptr", "ptr", "ptr", "i64", "ptr"})
    );

    auto thread_join = orison::lowering::concurrency_runtime_call(
        ConcurrencyRuntimeOperation::join_thread
    );
    assert(thread_join.symbol_name == "__orison_thread_join");
    assert(thread_join.return_type == "void");
    assert(thread_join.parameter_types == std::vector<std::string_view>({"ptr"}));

    auto task_spawn = orison::lowering::concurrency_runtime_call(
        ConcurrencyRuntimeOperation::spawn_task
    );
    assert(task_spawn.symbol_name == "__orison_task_spawn");
    assert(task_spawn.return_type == "ptr");
    assert(
        task_spawn.parameter_types ==
        std::vector<std::string_view>({"ptr", "ptr", "ptr", "i64", "ptr"})
    );

    auto task_await = orison::lowering::concurrency_runtime_call(
        ConcurrencyRuntimeOperation::await_task
    );
    assert(task_await.symbol_name == "__orison_task_await");
    assert(task_await.return_type == "void");
    assert(task_await.parameter_types == std::vector<std::string_view>({"ptr"}));

    auto destroy = orison::lowering::concurrency_runtime_call(
        ConcurrencyRuntimeOperation::destroy_handle
    );
    assert(destroy.symbol_name == "__orison_concurrency_handle_destroy");
    assert(destroy.return_type == "void");
    assert(destroy.parameter_types == std::vector<std::string_view>({"ptr"}));

    auto spawn_failed = orison::lowering::concurrency_runtime_call(
        ConcurrencyRuntimeOperation::spawn_failed
    );
    assert(spawn_failed.symbol_name == "__orison_concurrency_spawn_failed");
    assert(spawn_failed.return_type == "void");
    assert(spawn_failed.parameter_types.empty());
    return 0;
}
