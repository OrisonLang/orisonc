#pragma once

#include "orison/semantics/drop_model.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct FixtureDynamicArrayConstructionRequest {
    std::string_view source_type_name;
    std::string_view owner_name;
    std::size_t initial_capacity = 0;
};

struct LlvmIrEmissionOptions {
    // Test seam only. Do not expose this as user/compiler-driver surface.
    std::vector<std::string_view> test_only_declared_drop_source_type_allowlist;
    std::vector<FixtureDynamicArrayConstructionRequest> fixture_dynamic_array_construction_requests;
    bool fixture_derive_dynamic_array_cleanup_from_semantics = false;
    bool enable_dynamic_array_descriptor_cleanup_planning = false;
    bool fixture_enable_dynamic_array_parameter_descriptors = false;
    bool enable_dynamic_array_parameter_descriptor_audit_bindings = false;
    bool enable_dynamic_array_parameter_descriptors = false;
    bool enable_dynamic_array_construction_lowering = false;
    bool enable_dynamic_array_index_lowering = false;
    bool enable_dynamic_array_length_lowering = false;
    bool enable_dynamic_array_for_lowering = false;
    bool enable_dynamic_array_append_lowering = false;
    bool fixture_emit_bound_dynamic_array_parameter_cleanups = false;
    bool enable_dynamic_array_cleanup_emission = false;
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
    bool collect_computed_dynamic_array_for_descriptor_renders = false;
    bool collect_computed_dynamic_array_for_loop_control_renders = false;
    bool collect_computed_dynamic_array_for_element_address_renders = false;
    bool collect_computed_dynamic_array_for_element_load_renders = false;
    bool collect_computed_dynamic_array_for_loop_continue_renders = false;
    bool collect_computed_dynamic_array_for_loop_render_sequences = false;
    bool collect_computed_dynamic_array_for_loop_exit_cleanups = false;
    bool collect_computed_dynamic_array_for_cleanup_transitions = false;
    bool collect_computed_dynamic_array_for_production_emission_gates = false;
    bool collect_computed_dynamic_array_for_production_sequences = false;
    bool emit_computed_dynamic_array_for_production_sequence_comments = false;
    bool fixture_authorize_computed_dynamic_array_cleanup_calls = false;
    bool fixture_insert_computed_dynamic_array_cleanup_calls = false;
    bool enable_computed_dynamic_array_consumed_cleanup_descriptor_collection = false;
    bool enable_computed_dynamic_array_local_cleanup_call_insertion = true;
    bool suppress_computed_dynamic_array_cleanup_handoff_metadata = false;
    bool suppress_computed_dynamic_array_cleanup_operand_metadata = false;
    bool collect_aggregate_projection_access_metadata = false;
    bool enable_runtime_indexed_cleanup_emission = false;
    bool enable_runtime_indexed_cleanup_source_drop_emission = false;
    bool enable_runtime_indexed_constructor_move = false;
    bool enable_runtime_indexed_member_cleanup_ir_mutation_request = false;
    bool enable_runtime_indexed_member_cleanup_production_gate_request = false;
    bool enable_runtime_indexed_member_cleanup_apply_authorization_request = false;
    bool enable_runtime_indexed_member_cleanup_rewrite_execution_request = false;
    bool enable_runtime_indexed_fixed_array_constructor_move_only = false;
    std::vector<std::string> source_drop_definition_symbols;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;
};

struct ComputedDynamicArrayCleanupCallInsertionCapability {
    bool cleanup_call_authorization_enabled = false;
    bool cleanup_call_insertion_enabled = false;
    bool enabled = false;
};

inline auto computed_dynamic_array_cleanup_call_insertion_capability(
    LlvmIrEmissionOptions const& options
) -> ComputedDynamicArrayCleanupCallInsertionCapability {
    auto capability = ComputedDynamicArrayCleanupCallInsertionCapability {
        .cleanup_call_authorization_enabled =
            options.fixture_authorize_computed_dynamic_array_cleanup_calls ||
            options.enable_computed_dynamic_array_local_cleanup_call_insertion,
        .cleanup_call_insertion_enabled =
            options.fixture_insert_computed_dynamic_array_cleanup_calls ||
            options.enable_computed_dynamic_array_local_cleanup_call_insertion,
    };
    capability.enabled =
        capability.cleanup_call_authorization_enabled &&
        capability.cleanup_call_insertion_enabled;
    return capability;
}

inline auto dynamic_array_parameter_descriptors_enabled(
    LlvmIrEmissionOptions const& options
) -> bool {
    return options.enable_dynamic_array_parameter_descriptors ||
        options.fixture_enable_dynamic_array_parameter_descriptors;
}

inline auto dynamic_array_descriptor_cleanup_planning_enabled(
    LlvmIrEmissionOptions const& options
) -> bool {
    return options.enable_dynamic_array_descriptor_cleanup_planning ||
        options.fixture_derive_dynamic_array_cleanup_from_semantics;
}

inline auto dynamic_array_parameter_descriptor_audit_bindings_enabled(
    LlvmIrEmissionOptions const& options
) -> bool {
    return options.enable_dynamic_array_parameter_descriptor_audit_bindings;
}

inline auto dynamic_array_allocation_calls_enabled(
    LlvmIrEmissionOptions const& options
) -> bool {
    return options.enable_dynamic_array_construction_lowering ||
        options.test_only_render_dynamic_array_allocation_calls;
}

inline auto dynamic_array_cleanup_emission_enabled(
    LlvmIrEmissionOptions const& options
) -> bool {
    return options.enable_dynamic_array_cleanup_emission ||
        options.fixture_emit_bound_dynamic_array_parameter_cleanups;
}

}  // namespace orison::lowering
