#pragma once

#include "orison/semantics/drop_model.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct TestOnlyDynamicArrayConstructionRequest {
    std::string_view source_type_name;
    std::size_t initial_capacity = 0;
};

struct LlvmIrEmissionOptions {
    // Test seam only. Do not expose this as user/compiler-driver surface.
    std::vector<std::string_view> test_only_declared_drop_source_type_allowlist;
    std::vector<TestOnlyDynamicArrayConstructionRequest> test_only_dynamic_array_construction_requests;
    bool test_only_render_dynamic_array_allocation_calls = false;
    bool test_only_render_dynamic_array_descriptor_bindings = false;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;
};

}  // namespace orison::lowering
