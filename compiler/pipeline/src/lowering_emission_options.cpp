#include "lowering_emission_options.hpp"

#include "orison/semantics/drop_model.hpp"

namespace orison::pipeline {
namespace {

void authorize_dynamic_array_owned_element_source_drops(
    std::vector<semantics::DropLoweringAuthorization>& authorizations
) {
    for (auto& authorization : authorizations) {
        auto const expected_symbol_name = semantics::drop_abi_symbol_name(authorization.site.source_type_name);
        if (!authorization.semantic_resolved ||
            !authorization.site.owner_name.ends_with(".element") ||
            authorization.site.abi_symbol_name != expected_symbol_name) {
            continue;
        }

        authorization.source_drop_lowering_enabled = true;
        authorization.authorized = true;
    }
}

}  // namespace

auto dynamic_array_construction_lowering_enabled(CompilePipelineOptions const& options) -> bool {
    return options.dynamic_array_local_lowering_enabled ||
        options.dynamic_array_production_construction_lowering_enabled;
}

auto dynamic_array_parameter_lowering_enabled(CompilePipelineOptions const& options) -> bool {
    return options.dynamic_array_parameter_lowering_enabled ||
        options.dynamic_array_production_signature_lowering_enabled;
}

auto dynamic_array_index_lowering_enabled(CompilePipelineOptions const& options) -> bool {
    return options.dynamic_array_local_lowering_enabled ||
        options.dynamic_array_parameter_lowering_enabled ||
        options.dynamic_array_production_index_lowering_enabled;
}

auto dynamic_array_length_lowering_enabled(CompilePipelineOptions const& options) -> bool {
    return options.dynamic_array_local_lowering_enabled ||
        options.dynamic_array_parameter_lowering_enabled ||
        options.dynamic_array_production_length_lowering_enabled;
}

auto dynamic_array_for_lowering_enabled(CompilePipelineOptions const& options) -> bool {
    return options.dynamic_array_local_lowering_enabled ||
        options.dynamic_array_parameter_lowering_enabled ||
        options.dynamic_array_production_for_lowering_enabled;
}

auto dynamic_array_append_lowering_enabled(CompilePipelineOptions const& options) -> bool {
    return options.dynamic_array_local_lowering_enabled ||
        options.dynamic_array_production_append_lowering_enabled;
}

auto dynamic_array_cleanup_emission_enabled(CompilePipelineOptions const& options) -> bool {
    return options.dynamic_array_local_lowering_enabled ||
        options.dynamic_array_parameter_lowering_enabled ||
        options.dynamic_array_production_cleanup_emission_enabled;
}

auto source_drop_lowering_enabled(CompilePipelineOptions const& options) -> bool {
    return options.source_drop_lowering_enabled ||
        options.test_only_enable_source_drop_lowering;
}

auto dynamic_array_descriptor_cleanup_planning_enabled(CompilePipelineOptions const& options) -> bool {
    return options.dynamic_array_descriptor_cleanup_planning_enabled ||
        options.fixture_derive_dynamic_array_cleanup_from_semantics;
}

auto build_lowering_emission_options(
    CompilePipelineResult const& result,
    CompilePipelineOptions const& options,
    LoweringEmissionMode mode
) -> lowering::LlvmIrEmissionOptions {
    auto emission_options = lowering::LlvmIrEmissionOptions {};
    emission_options.semantic_drop_lowering_authorizations =
        options.test_only_semantic_drop_lowering_authorizations;
    emission_options.semantic_drop_lowering_authorizations.insert(
        emission_options.semantic_drop_lowering_authorizations.end(),
        result.semantic_drop_lowering_authorizations.begin(),
        result.semantic_drop_lowering_authorizations.end()
    );
    authorize_dynamic_array_owned_element_source_drops(emission_options.semantic_drop_lowering_authorizations);
    emission_options.fixture_dynamic_array_construction_requests =
        options.fixture_dynamic_array_construction_requests;
    emission_options.fixture_derive_dynamic_array_cleanup_from_semantics =
        options.fixture_derive_dynamic_array_cleanup_from_semantics;
    emission_options.collect_computed_dynamic_array_for_descriptor_renders =
        options.collect_computed_dynamic_array_for_descriptor_renders;
    emission_options.collect_computed_dynamic_array_for_loop_control_renders =
        options.collect_computed_dynamic_array_for_loop_control_renders;
    emission_options.collect_computed_dynamic_array_for_element_address_renders =
        options.collect_computed_dynamic_array_for_element_address_renders;
    emission_options.collect_computed_dynamic_array_for_element_load_renders =
        options.collect_computed_dynamic_array_for_element_load_renders;
    emission_options.collect_computed_dynamic_array_for_loop_continue_renders =
        options.collect_computed_dynamic_array_for_loop_continue_renders;
    emission_options.collect_computed_dynamic_array_for_loop_render_sequences =
        options.collect_computed_dynamic_array_for_loop_render_sequences;
    emission_options.collect_computed_dynamic_array_for_loop_exit_cleanups =
        options.collect_computed_dynamic_array_for_loop_exit_cleanups;
    emission_options.collect_computed_dynamic_array_for_cleanup_transitions =
        options.collect_computed_dynamic_array_for_cleanup_transitions;
    emission_options.collect_computed_dynamic_array_for_production_emission_gates =
        options.collect_computed_dynamic_array_for_production_emission_gates;
    emission_options.collect_computed_dynamic_array_for_production_sequences =
        options.collect_computed_dynamic_array_for_production_sequences;
    emission_options.emit_computed_dynamic_array_for_production_sequence_comments =
        options.emit_computed_dynamic_array_for_production_sequence_comments;
    emission_options.fixture_authorize_computed_dynamic_array_cleanup_calls =
        options.fixture_authorize_computed_dynamic_array_cleanup_calls;
    emission_options.fixture_insert_computed_dynamic_array_cleanup_calls =
        options.fixture_insert_computed_dynamic_array_cleanup_calls;
    emission_options.collect_aggregate_projection_access_metadata =
        options.collect_aggregate_projection_access_metadata;
    emission_options.enable_runtime_indexed_cleanup_emission =
        options.runtime_indexed_cleanup_emission_enabled;
    emission_options.enable_runtime_indexed_cleanup_source_drop_emission =
        options.runtime_indexed_cleanup_module_ir_insertion_enabled ||
        options.runtime_indexed_cleanup_module_ir_mutation_enabled ||
        options.runtime_indexed_cleanup_function_ir_module_rewrite_enabled ||
        options.runtime_indexed_cleanup_source_drop_emission_enabled;
    emission_options.enable_runtime_indexed_constructor_move =
        options.runtime_indexed_constructor_move_enabled;
    emission_options.enable_runtime_indexed_fixed_array_constructor_move_only =
        options.runtime_indexed_fixed_array_constructor_move_only;
    emission_options.enable_computed_dynamic_array_consumed_cleanup_descriptor_collection =
        options.dynamic_array_production_for_lowering_enabled &&
        dynamic_array_cleanup_emission_enabled(options);
    emission_options.enable_computed_dynamic_array_local_cleanup_call_insertion =
        options.computed_dynamic_array_local_cleanup_call_insertion_enabled;
    emission_options.suppress_computed_dynamic_array_cleanup_handoff_metadata =
        options.suppress_computed_dynamic_array_cleanup_handoff_metadata;
    emission_options.suppress_computed_dynamic_array_cleanup_operand_metadata =
        options.suppress_computed_dynamic_array_cleanup_operand_metadata;
    emission_options.enable_dynamic_array_descriptor_cleanup_planning =
        dynamic_array_descriptor_cleanup_planning_enabled(options);
    emission_options.enable_dynamic_array_parameter_descriptor_audit_bindings =
        options.dynamic_array_parameter_descriptor_audit_bindings_enabled;
    emission_options.enable_dynamic_array_parameter_descriptors =
        dynamic_array_parameter_lowering_enabled(options);
    emission_options.enable_dynamic_array_construction_lowering =
        dynamic_array_construction_lowering_enabled(options);
    emission_options.enable_dynamic_array_cleanup_emission =
        dynamic_array_cleanup_emission_enabled(options);

    if (mode == LoweringEmissionMode::full_ir) {
        emission_options.fixture_enable_dynamic_array_parameter_descriptors =
            options.fixture_enable_dynamic_array_parameter_descriptors;
        emission_options.enable_dynamic_array_index_lowering =
            dynamic_array_index_lowering_enabled(options);
        emission_options.enable_dynamic_array_length_lowering =
            dynamic_array_length_lowering_enabled(options);
        emission_options.enable_dynamic_array_for_lowering =
            dynamic_array_for_lowering_enabled(options);
        emission_options.enable_dynamic_array_append_lowering =
            dynamic_array_append_lowering_enabled(options);
        emission_options.fixture_emit_bound_dynamic_array_parameter_cleanups =
            options.fixture_emit_bound_dynamic_array_parameter_cleanups;
        emission_options.test_only_render_dynamic_array_element_drop_walks =
            options.test_only_render_dynamic_array_element_drop_walks;
    }

    return emission_options;
}

}  // namespace orison::pipeline
