#pragma once

#include <string_view>
#include <vector>

namespace orison::lowering {

enum class ConcurrencyRuntimeOperation {
    spawn_thread,
    join_thread,
    spawn_task,
    await_task,
    destroy_handle,
};

struct ConcurrencyRuntimeCall {
    std::string_view symbol_name;
    std::string_view return_type;
    std::vector<std::string_view> parameter_types;
};

auto concurrency_handle_llvm_type() -> std::string_view;

auto concurrency_runtime_call(
    ConcurrencyRuntimeOperation operation
) -> ConcurrencyRuntimeCall;

}  // namespace orison::lowering
