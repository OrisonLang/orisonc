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

enum class DynamicArrayDescriptorField {
    data,
    length,
    capacity,
};

enum class DynamicArrayBoundsCheckKind {
    index_within_length,
    append_has_capacity,
    length_within_capacity,
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

auto emit_dynamic_array_allocation_call(
    DynamicArrayConstructionPlan const& plan,
    std::string_view result_name
) -> std::string;

auto emit_dynamic_array_descriptor_binding(
    DynamicArrayConstructionPlan const& plan,
    std::string_view local_address_name,
    std::string_view descriptor_value_name
) -> std::string;

auto dynamic_array_descriptor_field_index(
    DynamicArrayDescriptorField field
) -> std::size_t;

auto dynamic_array_descriptor_field_llvm_type(
    DynamicArrayDescriptorField field
) -> std::string_view;

auto emit_dynamic_array_descriptor_field_projection(
    std::string_view result_name,
    std::string_view descriptor_value_name,
    DynamicArrayDescriptorField field
) -> std::string;

auto dynamic_array_bounds_check_predicate(
    DynamicArrayBoundsCheckKind kind
) -> std::string_view;

auto emit_dynamic_array_bounds_check(
    std::string_view result_name,
    std::string_view left_value_name,
    std::string_view right_value_name,
    DynamicArrayBoundsCheckKind kind
) -> std::string;

auto emit_dynamic_array_element_address(
    DynamicArrayConstructionPlan const& plan,
    std::string_view result_name,
    std::string_view data_pointer_name,
    std::string_view index_value_name
) -> std::string;

auto emit_dynamic_array_element_load(
    DynamicArrayConstructionPlan const& plan,
    std::string_view result_name,
    std::string_view element_address_name
) -> std::string;

auto emit_dynamic_array_element_store(
    DynamicArrayConstructionPlan const& plan,
    std::string_view value_name,
    std::string_view element_address_name
) -> std::string;

auto format_dynamic_array_runtime_request(
    DynamicArrayRuntimeOperation operation
) -> std::string;

auto format_dynamic_array_runtime_request_report(
    std::vector<DynamicArrayRuntimeOperation> const& operations
) -> std::vector<std::string>;

}  // namespace orison::lowering
