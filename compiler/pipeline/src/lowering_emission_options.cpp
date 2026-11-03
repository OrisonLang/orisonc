#include "lowering_emission_options.hpp"

#include "orison/semantics/drop_model.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace orison::pipeline {
namespace {

auto encoded_specialization_type_component(std::string_view value) -> std::string {
    constexpr auto hex_digits = std::string_view {"0123456789abcdef"};
    auto encoded = std::string {};
    encoded.reserve(value.size() * 2);
    for (auto character : value) {
        auto const byte = static_cast<unsigned char>(character);
        encoded += hex_digits[byte >> 4];
        encoded += hex_digits[byte & 0x0f];
    }
    return encoded;
}

auto specialization_type_local_name(
    semantics::GenericSpecializationTypeIdentity const& identity
) -> std::string {
    return "__orison_specialization_type_" +
        encoded_specialization_type_component(identity.package_name) + "_" +
        encoded_specialization_type_component(identity.declaration_name);
}

void append_specialization_type_declaration(
    semantics::GenericSpecializationTypeIdentity const& identity,
    std::string_view producer_package,
    std::vector<semantics::PackageInterface> const& package_interfaces,
    std::vector<semantics::ImportedTypeDeclarationMetadata>& imported_types,
    semantics::ImportedAggregateDeclarations& imported_aggregates
);

auto localize_specialization_layout_type(
    syntax::TypeSyntax type,
    semantics::PackageInterface const& package_interface,
    std::string_view producer_package,
    std::vector<semantics::PackageInterface> const& package_interfaces,
    std::vector<semantics::ImportedTypeDeclarationMetadata>& imported_types,
    semantics::ImportedAggregateDeclarations& imported_aggregates
) -> syntax::TypeSyntax {
    for (auto& argument : type.generic_arguments) {
        argument = localize_specialization_layout_type(
            std::move(argument),
            package_interface,
            producer_package,
            package_interfaces,
            imported_types,
            imported_aggregates
        );
    }

    auto declaration = std::ranges::find_if(
        package_interface.exported_type_declarations,
        [&](semantics::PackageTypeDeclaration const& candidate) {
            return candidate.name == type.name &&
                (candidate.kind == semantics::PackageTypeDeclarationKind::record ||
                 candidate.kind == semantics::PackageTypeDeclarationKind::choice);
        }
    );
    if (declaration == package_interface.exported_type_declarations.end()) {
        return type;
    }

    auto identity = semantics::GenericSpecializationTypeIdentity {
        .package_name = package_interface.package_name,
        .declaration_name = declaration->name,
    };
    append_specialization_type_declaration(
        identity,
        producer_package,
        package_interfaces,
        imported_types,
        imported_aggregates
    );
    auto localized = std::ranges::find_if(
        imported_types,
        [&](semantics::ImportedTypeDeclarationMetadata const& candidate) {
            return candidate.package_name == identity.package_name &&
                candidate.declaration_name == identity.declaration_name;
        }
    );
    if (localized != imported_types.end()) {
        type.name = localized->local_name;
    }
    return type;
}

void append_specialization_type_declaration(
    semantics::GenericSpecializationTypeIdentity const& identity,
    std::string_view producer_package,
    std::vector<semantics::PackageInterface> const& package_interfaces,
    std::vector<semantics::ImportedTypeDeclarationMetadata>& imported_types,
    semantics::ImportedAggregateDeclarations& imported_aggregates
) {
    for (auto const& argument : identity.generic_arguments) {
        append_specialization_type_declaration(
            argument,
            producer_package,
            package_interfaces,
            imported_types,
            imported_aggregates
        );
    }
    if (identity.package_name.empty() || identity.package_name == producer_package) {
        return;
    }
    if (std::ranges::any_of(
            imported_types,
            [&](semantics::ImportedTypeDeclarationMetadata const& declaration) {
                return declaration.package_name == identity.package_name &&
                    declaration.declaration_name == identity.declaration_name;
            }
        )) {
        return;
    }

    auto package_interface = std::ranges::find_if(
        package_interfaces,
        [&](semantics::PackageInterface const& candidate) {
            return candidate.package_name == identity.package_name;
        }
    );
    if (package_interface == package_interfaces.end()) {
        return;
    }
    auto declaration = std::ranges::find_if(
        package_interface->exported_type_declarations,
        [&](semantics::PackageTypeDeclaration const& candidate) {
            return candidate.name == identity.declaration_name;
        }
    );
    if (declaration == package_interface->exported_type_declarations.end()) {
        return;
    }
    if (declaration->kind != semantics::PackageTypeDeclarationKind::record &&
        declaration->kind != semantics::PackageTypeDeclarationKind::choice) {
        return;
    }

    auto const local_name = specialization_type_local_name(identity);
    imported_types.push_back(semantics::ImportedTypeDeclarationMetadata {
        .package_name = identity.package_name,
        .declaration_name = identity.declaration_name,
        .local_name = local_name,
        .generic_arity = declaration->generic_arity,
    });
    if (declaration->kind == semantics::PackageTypeDeclarationKind::record) {
        auto record = syntax::RecordSyntax {
            .visibility = syntax::Visibility::public_visibility,
            .name = local_name,
            .generic_parameters = declaration->generic_parameters,
            .fields = declaration->public_record_fields,
        };
        for (auto& field : record.fields) {
            field.type = localize_specialization_layout_type(
                std::move(field.type),
                *package_interface,
                producer_package,
                package_interfaces,
                imported_types,
                imported_aggregates
            );
        }
        imported_aggregates.records.push_back(std::move(record));
    } else if (declaration->kind == semantics::PackageTypeDeclarationKind::choice) {
        auto choice = syntax::ChoiceSyntax {
            .visibility = syntax::Visibility::public_visibility,
            .name = local_name,
            .generic_parameters = declaration->generic_parameters,
            .variants = declaration->choice_variants,
        };
        for (auto& variant : choice.variants) {
            for (auto& payload : variant.payloads) {
                payload.type = localize_specialization_layout_type(
                    std::move(payload.type),
                    *package_interface,
                    producer_package,
                    package_interfaces,
                    imported_types,
                    imported_aggregates
                );
            }
        }
        imported_aggregates.choices.push_back(std::move(choice));
    }
}

void append_specialization_type_declarations(
    syntax::ModuleSyntax const& module,
    std::vector<semantics::GenericFunctionSpecializationRequest> const& requests,
    std::vector<semantics::PackageInterface> const& package_interfaces,
    std::vector<semantics::ImportedTypeDeclarationMetadata>& imported_types,
    semantics::ImportedAggregateDeclarations& imported_aggregates
) {
    for (auto const& request : requests) {
        if (request.package_name != module.package_name) {
            continue;
        }
        for (auto const& identity : request.type_identities) {
            append_specialization_type_declaration(
                identity,
                module.package_name,
                package_interfaces,
                imported_types,
                imported_aggregates
            );
        }
    }
}

void authorize_dynamic_array_owned_element_cleanups(
    std::vector<semantics::OwnedCleanupLoweringAuthorization>& authorizations
) {
    for (auto& authorization : authorizations) {
        if (!authorization.semantic_resolved ||
            !authorization.site.owner_name.ends_with(".element") ||
            authorization.site.abi_symbol_name.empty()) {
            continue;
        }

        authorization.semantic_owned_cleanup_lowering_enabled = true;
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

auto semantic_owned_cleanup_lowering_enabled(CompilePipelineOptions const& options) -> bool {
    return options.semantic_owned_cleanup_lowering_enabled ||
        options.test_only_enable_semantic_owned_cleanup_lowering;
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
    emission_options.generic_function_specialization_requests =
        options.generic_function_specialization_requests;
    emission_options.imported_type_declarations = semantics::resolve_imported_type_declarations(
        result.parse_result.module,
        options.available_package_interfaces
    );
    auto imported_aggregates = semantics::resolve_imported_aggregate_declarations(
        result.parse_result.module,
        options.available_package_interfaces
    );
    append_specialization_type_declarations(
        result.parse_result.module,
        options.generic_function_specialization_requests,
        options.available_package_interfaces,
        emission_options.imported_type_declarations,
        imported_aggregates
    );
    emission_options.imported_record_declarations = std::move(imported_aggregates.records);
    emission_options.imported_choice_declarations = std::move(imported_aggregates.choices);
    emission_options.semantic_owned_cleanup_lowering_authorizations =
        options.test_only_semantic_owned_cleanup_lowering_authorizations;
    emission_options.semantic_owned_cleanup_lowering_authorizations.insert(
        emission_options.semantic_owned_cleanup_lowering_authorizations.end(),
        result.semantic_owned_cleanup_lowering_authorizations.begin(),
        result.semantic_owned_cleanup_lowering_authorizations.end()
    );
    authorize_dynamic_array_owned_element_cleanups(emission_options.semantic_owned_cleanup_lowering_authorizations);
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
        options.collect_computed_dynamic_array_for_production_sequences ||
        options.computed_dynamic_array_production_module_ir_mutation_enabled;
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
    emission_options.enable_runtime_indexed_cleanup_semantic_owned_cleanup_emission =
        options.runtime_indexed_cleanup_module_ir_insertion_enabled ||
        options.runtime_indexed_cleanup_module_ir_mutation_enabled ||
        options.runtime_indexed_cleanup_function_ir_module_rewrite_enabled ||
        options.runtime_indexed_cleanup_semantic_owned_cleanup_emission_enabled;
    emission_options.enable_runtime_indexed_constructor_move =
        options.runtime_indexed_constructor_move_enabled;
    emission_options.enable_runtime_indexed_member_cleanup_ir_mutation_request =
        options.runtime_indexed_member_cleanup_ir_mutation_enabled;
    emission_options.enable_runtime_indexed_member_cleanup_production_gate_request =
        options.runtime_indexed_member_cleanup_production_gate_enabled;
    emission_options.enable_runtime_indexed_member_cleanup_apply_authorization_request =
        options.runtime_indexed_member_cleanup_apply_authorization_enabled;
    emission_options.enable_runtime_indexed_member_cleanup_rewrite_execution_request =
        options.runtime_indexed_member_cleanup_rewrite_execution_enabled;
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
        emission_options.test_only_render_dynamic_array_element_owned_cleanup_walks =
            options.test_only_render_dynamic_array_element_owned_cleanup_walks;
    }

    return emission_options;
}

}  // namespace orison::pipeline
