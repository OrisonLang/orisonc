#pragma once

#include <string>
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

auto format_dynamic_array_runtime_request(
    DynamicArrayRuntimeOperation operation
) -> std::string;

auto format_dynamic_array_runtime_request_report(
    std::vector<DynamicArrayRuntimeOperation> const& operations
) -> std::vector<std::string>;

}  // namespace orison::lowering
