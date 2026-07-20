#include "lowering_emission_options.hpp"

namespace orison::pipeline {

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
        options.test_only_derive_dynamic_array_cleanup_from_semantics;
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
    emission_options.test_only_dynamic_array_construction_requests =
        options.test_only_dynamic_array_construction_requests;
    emission_options.test_only_derive_dynamic_array_cleanup_from_semantics =
        options.test_only_derive_dynamic_array_cleanup_from_semantics;
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
        emission_options.test_only_enable_dynamic_array_parameter_descriptors =
            options.test_only_enable_dynamic_array_parameter_descriptors;
        emission_options.enable_dynamic_array_index_lowering =
            dynamic_array_index_lowering_enabled(options);
        emission_options.enable_dynamic_array_length_lowering =
            dynamic_array_length_lowering_enabled(options);
        emission_options.enable_dynamic_array_for_lowering =
            dynamic_array_for_lowering_enabled(options);
        emission_options.enable_dynamic_array_append_lowering =
            dynamic_array_append_lowering_enabled(options);
        emission_options.test_only_emit_bound_dynamic_array_parameter_cleanups =
            options.test_only_emit_bound_dynamic_array_parameter_cleanups;
        emission_options.test_only_render_dynamic_array_element_drop_walks =
            options.test_only_render_dynamic_array_element_drop_walks;
    }

    return emission_options;
}

}  // namespace orison::pipeline
