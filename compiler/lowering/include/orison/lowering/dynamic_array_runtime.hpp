#pragma once

#include "orison/lowering/target_layout.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct LoweringContext;

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

struct DynamicArrayConstructionPlan {
    std::string source_type_name;
    std::string element_source_type_name;
    std::string element_llvm_type;
    std::size_t element_size_bytes = 0;
    std::size_t initial_capacity = 0;
    DynamicArrayRuntimeOperation operation = DynamicArrayRuntimeOperation::allocate;
};

auto dynamic_array_runtime_call(
    DynamicArrayRuntimeOperation operation
) -> DynamicArrayRuntimeCall;

auto plan_dynamic_array_construction(
    std::string_view source_type_name,
    std::size_t initial_capacity,
    LoweringContext const& context,
    TargetLayout const& layout = native_target_layout()
) -> std::optional<DynamicArrayConstructionPlan>;

auto format_dynamic_array_construction_plan(
    DynamicArrayConstructionPlan const& plan
) -> std::string;

auto format_dynamic_array_construction_plan_report(
    std::vector<DynamicArrayConstructionPlan> const& plans
) -> std::vector<std::string>;

auto format_dynamic_array_runtime_request(
    DynamicArrayRuntimeOperation operation
) -> std::string;

auto format_dynamic_array_runtime_request_report(
    std::vector<DynamicArrayRuntimeOperation> const& operations
) -> std::vector<std::string>;

}  // namespace orison::lowering
