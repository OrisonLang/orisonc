#pragma once

#include <string_view>
#include <vector>

namespace orison::lowering {

enum class DynamicArrayRuntimeOperation {
    allocate,
    grow,
    deallocate,
};

struct DynamicArrayRuntimeCall {
    std::string_view symbol_name;
    std::string_view return_type;
    std::vector<std::string_view> parameter_types;
};

auto dynamic_array_runtime_call(
    DynamicArrayRuntimeOperation operation
) -> DynamicArrayRuntimeCall;

}  // namespace orison::lowering
