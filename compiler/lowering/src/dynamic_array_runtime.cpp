#include "orison/lowering/dynamic_array_runtime.hpp"

#include "orison/lowering/source_type_queries.hpp"

namespace orison::lowering {

auto dynamic_array_runtime_call(
    DynamicArrayRuntimeOperation operation
) -> DynamicArrayRuntimeCall {
    switch (operation) {
    case DynamicArrayRuntimeOperation::allocate:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_allocate",
            .return_type = dynamic_array_descriptor_llvm_type(),
            .parameter_types = {"i64", "i64"},
        };
    case DynamicArrayRuntimeOperation::grow:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_grow",
            .return_type = dynamic_array_descriptor_llvm_type(),
            .parameter_types = {dynamic_array_descriptor_llvm_type(), "i64", "i64"},
        };
    case DynamicArrayRuntimeOperation::deallocate:
        return DynamicArrayRuntimeCall {
            .symbol_name = "__orison_dynamic_array_deallocate",
            .return_type = "void",
            .parameter_types = {"ptr", "i64", "i64"},
        };
    }
    return {};
}

}  // namespace orison::lowering
