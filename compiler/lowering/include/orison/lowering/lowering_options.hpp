#pragma once

#include "orison/semantics/drop_model.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct TestOnlyDynamicArrayConstructionRequest {
    std::string_view source_type_name;
    std::string_view owner_name;
    std::size_t initial_capacity = 0;
};

struct LlvmIrEmissionOptions {
    // Test seam only. Do not expose this as user/compiler-driver surface.
    std::vector<std::string_view> test_only_declared_drop_source_type_allowlist;
    std::vector<TestOnlyDynamicArrayConstructionRequest> test_only_dynamic_array_construction_requests;
    bool test_only_derive_dynamic_array_cleanup_from_semantics = false;
    bool test_only_render_dynamic_array_allocation_calls = false;
    bool test_only_render_dynamic_array_grow_calls = false;
    bool test_only_render_dynamic_array_deallocation_calls = false;
    bool test_only_render_dynamic_array_descriptor_bindings = false;
    bool test_only_render_dynamic_array_descriptor_projections = false;
    bool test_only_render_dynamic_array_bounds_checks = false;
    bool test_only_render_dynamic_array_element_addresses = false;
    bool test_only_render_dynamic_array_element_loads = false;
    bool test_only_render_dynamic_array_element_stores = false;
    bool test_only_render_dynamic_array_descriptor_length_updates = false;
    bool test_only_render_dynamic_array_descriptor_write_backs = false;
    bool test_only_render_dynamic_array_append_sequences = false;
    bool test_only_render_dynamic_array_grow_sequences = false;
    bool test_only_render_dynamic_array_append_with_grow_sequences = false;
    bool test_only_render_dynamic_array_cleanup_sequences = false;
    bool test_only_render_dynamic_array_descriptor_load_cleanup_sequences = false;
    bool test_only_render_dynamic_array_element_drop_walks = false;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;
};

}  // namespace orison::lowering
