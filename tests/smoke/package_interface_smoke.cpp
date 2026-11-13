#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/semantics/package_interface.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <string>

int main() {
    auto producer = orison::syntax::ModuleSyntax {};
    producer.package_name = "external.types";
    producer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "Detail",
        .alias = "ExternalDetail",
        .from_package = "external.shared",
    });
    producer.type_aliases.push_back(orison::syntax::TypeAliasSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .name = "Identifier",
        .aliased_type = {.name = "UInt64"},
    });
    producer.type_aliases.push_back(orison::syntax::TypeAliasSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .name = "SharedDetail",
        .aliased_type = {.name = "ExternalDetail"},
    });
    producer.type_aliases.push_back(orison::syntax::TypeAliasSyntax {
        .name = "InternalIdentifier",
        .aliased_type = {.name = "UInt64"},
    });
    producer.records.push_back(orison::syntax::RecordSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .name = "Pair",
        .generic_parameters = {"Left", "Right"},
        .fields = {
            orison::syntax::FieldSyntax {
                .visibility = orison::syntax::Visibility::public_visibility,
                .name = "first",
                .type = {.name = "Left"},
            },
            orison::syntax::FieldSyntax {
                .name = "hidden",
                .type = {.name = "Right"},
            },
            orison::syntax::FieldSyntax {
                .visibility = orison::syntax::Visibility::public_visibility,
                .name = "detail",
                .type = {.name = "ExternalDetail"},
            },
        },
    });
    producer.records.push_back(orison::syntax::RecordSyntax {.name = "InternalRecord"});
    producer.choices.push_back(orison::syntax::ChoiceSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .name = "Result",
        .generic_parameters = {"Value"},
        .variants = {
            orison::syntax::ChoiceVariantSyntax {
                .name = "Ok",
                .payloads = {{.name = "value", .type = {.name = "Value"}}},
            },
            orison::syntax::ChoiceVariantSyntax {
                .name = "Error",
                .payloads = {{.name = "reason", .type = {.name = "ExternalDetail"}}},
            },
        },
    });
    producer.interfaces.push_back(orison::syntax::InterfaceSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .name = "Reader",
    });
    producer.functions.push_back(orison::syntax::FunctionSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .is_async = true,
        .is_unsafe = true,
        .name = "transform",
        .generic_parameters = {"Value"},
        .parameters = {
            orison::syntax::ParameterSyntax {
                .name = "values",
                .type = {
                    .name = "shared.View",
                    .generic_arguments = {{.name = "Value"}},
                },
            },
        },
        .return_type = {
            .name = "Result",
            .generic_arguments = {{.name = "Value"}},
        },
        .where_constraints = {
            orison::syntax::WhereConstraintSyntax {
                .parameter_name = "Value",
                .requirements = {{.name = "Reader"}, {.name = "Shareable"}},
            },
        },
    });
    producer.functions.push_back(orison::syntax::FunctionSyntax {
        .name = "internal_transform",
        .return_type = {.name = "Unit"},
    });
    producer.constants.push_back(orison::syntax::ConstantSyntax {
        .name = "BASE",
        .type = {.name = "UInt32"},
        .initializer = {
            .kind = orison::syntax::ExpressionKind::integer_literal,
            .text = "40",
        },
    });
    auto answer_initializer = orison::syntax::ExpressionSyntax {
        .kind = orison::syntax::ExpressionKind::binary,
        .text = "+",
        .left = std::make_unique<orison::syntax::ExpressionSyntax>(orison::syntax::ExpressionSyntax {
            .kind = orison::syntax::ExpressionKind::name,
            .text = "BASE",
        }),
        .right = std::make_unique<orison::syntax::ExpressionSyntax>(orison::syntax::ExpressionSyntax {
            .kind = orison::syntax::ExpressionKind::integer_literal,
            .text = "2",
        }),
    };
    producer.constants.push_back(orison::syntax::ConstantSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .name = "ANSWER",
        .type = {.name = "UInt32"},
        .initializer = std::move(answer_initializer),
    });

    auto package_interface = orison::semantics::build_package_interface(producer);
    assert(package_interface.package_name == "external.types");
    assert(package_interface.exported_type_declarations.size() == 5);

    auto find_export = [&](std::string const& name) {
        return std::ranges::find_if(
            package_interface.exported_type_declarations,
            [&](orison::semantics::PackageTypeDeclaration const& declaration) {
                return declaration.name == name;
            }
        );
    };
    auto identifier = find_export("Identifier");
    assert(identifier != package_interface.exported_type_declarations.end());
    assert(identifier->kind == orison::semantics::PackageTypeDeclarationKind::type_alias);
    assert(identifier->generic_arity == 0);
    assert(identifier->aliased_type_identity.has_value());
    assert(identifier->aliased_type_identity->declaration_name == "UInt64");
    auto shared_detail = find_export("SharedDetail");
    assert(shared_detail != package_interface.exported_type_declarations.end());
    assert(shared_detail->aliased_type_identity.has_value());
    assert(shared_detail->aliased_type_identity->package_name == "external.shared");
    assert(shared_detail->aliased_type_identity->declaration_name == "Detail");
    auto pair = find_export("Pair");
    assert(pair != package_interface.exported_type_declarations.end());
    assert(pair->kind == orison::semantics::PackageTypeDeclarationKind::record);
    assert(pair->generic_arity == 2);
    assert(pair->generic_parameters == std::vector<std::string>({"Left", "Right"}));
    assert(pair->public_record_fields.size() == 2);
    assert(pair->public_record_fields.front().name == "first");
    assert(pair->public_record_field_type_identities[1].package_name == "external.shared");
    assert(pair->public_record_field_type_identities[1].declaration_name == "Detail");
    auto result = find_export("Result");
    assert(result != package_interface.exported_type_declarations.end());
    assert(result->kind == orison::semantics::PackageTypeDeclarationKind::choice);
    assert(result->generic_arity == 1);
    assert(result->choice_variants.size() == 2);
    auto reader = find_export("Reader");
    assert(reader != package_interface.exported_type_declarations.end());
    assert(reader->kind == orison::semantics::PackageTypeDeclarationKind::interface_declaration);
    assert(reader->generic_arity == 0);
    assert(find_export("InternalIdentifier") == package_interface.exported_type_declarations.end());
    assert(find_export("InternalRecord") == package_interface.exported_type_declarations.end());
    assert(package_interface.exported_functions.size() == 1);
    assert(package_interface.exported_functions.front().name == "transform");
    assert(package_interface.exported_constants.size() == 1);
    assert(package_interface.exported_constants.front().name == "ANSWER");
    assert(package_interface.exported_constants.front().initializer_text == "(40 + 2)");

    auto serialized = orison::semantics::serialize_package_interface(package_interface);
    assert(
        serialized ==
        "orison-package-interface 1\n"
        "package external.types\n"
        "type alias Identifier 0\n"
        "alias Identifier UInt64 UInt64\n"
        "type record Pair 2 Left Right\n"
        "field Pair first Left Left\n"
        "field Pair detail ExternalDetail external.shared::Detail\n"
        "type interface Reader 0\n"
        "type choice Result 1 Value\n"
        "variant Result Ok 1 value Value Value\n"
        "variant Result Error 1 reason ExternalDetail external.shared::Detail\n"
        "type alias SharedDetail 0\n"
        "alias SharedDetail ExternalDetail external.shared::Detail\n"
        "constant ANSWER UInt32 UInt32 (40 + 2)\n"
        "function transform 1 Value 1 values shared.View<Value> Result<Value> async unsafe 1 Value 2 Reader Shareable\n"
    );
    auto parsed = orison::semantics::parse_package_interface(serialized);
    assert(!parsed.has_error());
    assert(parsed.package_interface.has_value());
    assert(orison::semantics::serialize_package_interface(*parsed.package_interface) == serialized);
    assert(parsed.package_interface->exported_functions.size() == 1);
    assert(parsed.package_interface->exported_functions.front().is_async);
    assert(parsed.package_interface->exported_functions.front().is_unsafe);
    assert(parsed.package_interface->exported_functions.front().where_constraints.size() == 1);
    assert(parsed.package_interface->exported_constants.size() == 1);
    auto imported_constant = orison::semantics::ImportedConstantDeclaration {
        .package_name = "external.types",
        .declaration_name = "ANSWER",
        .local_name = "ExternalAnswer",
        .type = {.name = "UInt32"},
        .initializer_text = parsed.package_interface->exported_constants.front().initializer_text,
    };
    auto materialized_constant = orison::semantics::materialize_imported_constant(imported_constant);
    assert(materialized_constant.has_value());
    assert(materialized_constant->name == "ExternalAnswer");
    assert(materialized_constant->initializer.kind == orison::syntax::ExpressionKind::binary);

    auto empty = orison::semantics::parse_package_interface("");
    assert(empty.has_error());
    assert(empty.error_message == "package interface is empty");
    auto unsupported_version = orison::semantics::parse_package_interface(
        "orison-package-interface 2\npackage external.types\n"
    );
    assert(unsupported_version.has_error());
    assert(unsupported_version.error_message == "unsupported package interface version '2'");
    auto unknown_kind = orison::semantics::parse_package_interface(
        "orison-package-interface 1\npackage external.types\ntype unknown Pair 2 Left Right\n"
    );
    assert(unknown_kind.has_error());
    assert(unknown_kind.error_message == "unknown package interface type kind 'unknown' at line 3");
    auto invalid_arity = orison::semantics::parse_package_interface(
        "orison-package-interface 1\npackage external.types\ntype record Pair two\n"
    );
    assert(invalid_arity.has_error());
    assert(invalid_arity.error_message == "invalid package interface generic arity 'two' at line 3");
    auto duplicate = orison::semantics::parse_package_interface(
        "orison-package-interface 1\n"
        "package external.types\n"
        "type record Pair 2 Left Right\n"
        "type choice Pair 1 Value\n"
    );
    assert(duplicate.has_error());
    assert(duplicate.error_message == "duplicate package interface type declaration 'Pair' at line 4");
    auto invalid_field = orison::semantics::parse_package_interface(
        "orison-package-interface 1\n"
        "package external.types\n"
        "type record Pair 2 Left Right\n"
        "field Pair first View< View<\n"
    );
    assert(invalid_field.has_error());
    assert(invalid_field.error_message == "invalid package interface field declaration at line 4");
    auto missing_alias_target = orison::semantics::parse_package_interface(
        "orison-package-interface 1\npackage external.types\ntype alias Identifier 0\n"
    );
    assert(missing_alias_target.has_error());
    assert(missing_alias_target.error_message == "package interface alias 'Identifier' is missing its target");
    auto invalid_function = orison::semantics::parse_package_interface(
        "orison-package-interface 1\n"
        "package external.types\n"
        "function transform 0 1 value UInt32 UInt32 sometimes safe 0\n"
    );
    assert(invalid_function.has_error());
    assert(invalid_function.error_message == "invalid package interface function declaration at line 3");
    auto invalid_constant = orison::semantics::parse_package_interface(
        "orison-package-interface 1\n"
        "package external.types\n"
        "constant ANSWER UInt32 UInt32\n"
    );
    assert(invalid_constant.has_error());
    assert(invalid_constant.error_message == "invalid package interface constant declaration at line 3");

    auto specialization_requests = std::vector<orison::semantics::GenericFunctionSpecializationRequest> {
        {
            .package_name = "external.generics",
            .declaration_name = "choose",
            .type_arguments = {{.name = "UInt32"}},
        },
        {
            .package_name = "external.generics",
            .declaration_name = "combine",
            .type_arguments = {
                {
                    .name = "DynamicArray",
                    .generic_arguments = {{.name = "ExternalPair"}},
                },
                {.name = "UInt64"},
            },
            .type_identities = {
                {
                    .declaration_name = "DynamicArray",
                    .generic_arguments = {{
                        .package_name = "external.types",
                        .declaration_name = "Pair",
                    }},
                },
                {.declaration_name = "UInt64"},
            },
        },
        {
            .package_name = "external.generics",
            .declaration_name = "choose",
            .type_arguments = {{.name = "UInt32"}},
        },
    };
    auto serialized_specialization_requests =
        orison::semantics::serialize_generic_function_specialization_requests(specialization_requests);
    assert(
        serialized_specialization_requests ==
        "orison-specialization-requests 1\n"
        "request external.generics choose 1 UInt32 UInt32\n"
        "request external.generics combine 2 DynamicArray<Pair> "
            "DynamicArray<external.types::Pair> UInt64 UInt64\n"
    );
    auto parsed_specialization_requests =
        orison::semantics::parse_generic_function_specialization_requests(
            serialized_specialization_requests
        );
    assert(!parsed_specialization_requests.has_error());
    assert(parsed_specialization_requests.requests.size() == 2);
    assert(parsed_specialization_requests.requests[1].type_identities[0].generic_arguments[0].package_name ==
        "external.types");
    assert(
        orison::semantics::serialize_generic_function_specialization_requests(
            parsed_specialization_requests.requests
        ) == serialized_specialization_requests
    );

    auto empty_specialization_requests =
        orison::semantics::parse_generic_function_specialization_requests("");
    assert(empty_specialization_requests.has_error());
    assert(
        empty_specialization_requests.error_message ==
        "specialization request artifact is empty"
    );
    auto unsupported_specialization_request_version =
        orison::semantics::parse_generic_function_specialization_requests(
            "orison-specialization-requests 2\n"
        );
    assert(unsupported_specialization_request_version.has_error());
    assert(
        unsupported_specialization_request_version.error_message ==
        "unsupported specialization request version '2'"
    );
    auto invalid_specialization_request_count =
        orison::semantics::parse_generic_function_specialization_requests(
            "orison-specialization-requests 1\n"
            "request external.generics choose 0\n"
        );
    assert(invalid_specialization_request_count.has_error());
    assert(
        invalid_specialization_request_count.error_message ==
        "invalid specialization request type argument count at line 2"
    );
    auto invalid_specialization_request_type =
        orison::semantics::parse_generic_function_specialization_requests(
            "orison-specialization-requests 1\n"
            "request external.generics choose 1 View< UInt32\n"
        );
    assert(invalid_specialization_request_type.has_error());
    assert(
        invalid_specialization_request_type.error_message ==
        "invalid specialization request type at line 2"
    );
    auto duplicate_specialization_request =
        orison::semantics::parse_generic_function_specialization_requests(
            "orison-specialization-requests 1\n"
            "request external.generics choose 1 UInt32 UInt32\n"
            "request external.generics choose 1 UInt32 UInt32\n"
        );
    assert(duplicate_specialization_request.has_error());
    assert(
        duplicate_specialization_request.error_message ==
        "duplicate specialization request at line 3"
    );

    auto consumer = orison::syntax::ModuleSyntax {};
    consumer.package_name = "demo.consumer";
    consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "Pair",
        .alias = "ExternalPair",
        .from_package = "external.types",
    });
    consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "Result",
        .alias = "ExternalResult",
        .from_package = "external.types",
    });
    consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "SharedDetail",
        .alias = "ExternalSharedDetail",
        .from_package = "external.types",
    });
    consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "Identifier",
        .alias = "ExternalIdentifier",
        .from_package = "external.types",
    });
    consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "transform",
        .alias = "external_transform",
        .from_package = "external.types",
    });
    consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "Missing",
        .from_package = "external.types",
    });
    consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "Pair",
        .from_package = "unrelated.types",
    });
    consumer.functions.push_back(orison::syntax::FunctionSyntax {
        .line = 4,
        .name = "consume",
        .parameters = {
            orison::syntax::ParameterSyntax {
                .line = 4,
                .name = "value",
                .type = {
                    .name = "ExternalPair",
                    .generic_arguments = {{.name = "UInt32"}},
                },
            },
        },
        .return_type = {.name = "Unit"},
    });

    auto shared_interface = orison::semantics::PackageInterface {
        .package_name = "external.shared",
        .exported_type_declarations = {{
            .kind = orison::semantics::PackageTypeDeclarationKind::record,
            .name = "Detail",
            .public_record_fields = {{
                .visibility = orison::syntax::Visibility::public_visibility,
                .name = "value",
                .type = {.name = "UInt32"},
            }},
        }},
    };
    auto available_interfaces = std::vector<orison::semantics::PackageInterface> {
        package_interface,
        shared_interface,
    };
    auto imported_declarations = orison::semantics::resolve_imported_type_declarations(
        consumer,
        available_interfaces
    );
    assert(imported_declarations.size() == 4);
    assert(imported_declarations.front().package_name == "external.types");
    assert(imported_declarations.front().declaration_name == "Pair");
    assert(imported_declarations.front().local_name == "ExternalPair");
    assert(imported_declarations.front().generic_arity == 2);
    auto imported_identifier = std::ranges::find_if(
        imported_declarations,
        [](orison::semantics::ImportedTypeDeclarationMetadata const& declaration) {
            return declaration.local_name == "ExternalIdentifier";
        }
    );
    assert(imported_identifier != imported_declarations.end());
    assert(imported_identifier->lowering_alias_target.has_value());
    assert(imported_identifier->lowering_alias_target->name == "UInt64");

    auto maybe_record_interface = orison::semantics::parse_package_interface(
        "orison-package-interface 1\n"
        "package external.maybe_record\n"
        "type record Detail 0\n"
        "field Detail value UInt32 UInt32\n"
        "type choice Maybe 1 T\n"
        "variant Maybe Some 1 value T T\n"
        "variant Maybe Empty 0\n"
        "type alias PublicMaybeDetail 0\n"
        "alias PublicMaybeDetail Maybe<Detail> external.maybe_record::Maybe<external.maybe_record::Detail>\n"
    );
    assert(!maybe_record_interface.has_error());
    auto maybe_record_consumer = orison::syntax::ModuleSyntax {};
    maybe_record_consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "Detail",
        .alias = "ExternalDetailRecord",
        .from_package = "external.maybe_record",
    });
    maybe_record_consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "PublicMaybeDetail",
        .alias = "ExternalMaybeDetail",
        .from_package = "external.maybe_record",
    });
    auto maybe_record_imports = orison::semantics::resolve_imported_type_declarations(
        maybe_record_consumer,
        {*maybe_record_interface.package_interface}
    );
    auto maybe_record_alias = std::ranges::find_if(
        maybe_record_imports,
        [](orison::semantics::ImportedTypeDeclarationMetadata const& declaration) {
            return declaration.local_name == "ExternalMaybeDetail";
        }
    );
    assert(maybe_record_alias != maybe_record_imports.end());
    assert(maybe_record_alias->lowering_alias_target.has_value());
    assert(maybe_record_alias->lowering_alias_target->name == "Maybe");
    assert(maybe_record_alias->lowering_alias_target->generic_arguments.size() == 1);
    assert(maybe_record_alias->lowering_alias_target->generic_arguments.front().name == "ExternalDetailRecord");

    auto imported_aggregates = orison::semantics::resolve_imported_aggregate_declarations(
        consumer,
        available_interfaces
    );
    assert(imported_aggregates.records.size() == 2);
    assert(imported_aggregates.records.front().name == "ExternalPair");
    assert(imported_aggregates.records.front().generic_parameters == std::vector<std::string>({"Left", "Right"}));
    assert(imported_aggregates.records.front().fields.size() == 2);
    assert(imported_aggregates.records.front().fields.front().name == "first");
    assert(imported_aggregates.records[1].name == "ExternalSharedDetail");
    assert(imported_aggregates.records[1].fields.size() == 1);
    assert(imported_aggregates.records[1].fields.front().name == "value");
    assert(imported_aggregates.choices.size() == 1);
    assert(imported_aggregates.choices.front().name == "ExternalResult");
    assert(imported_aggregates.choices.front().variants.size() == 2);

    auto cyclic_interface = orison::semantics::PackageInterface {
        .package_name = "external.cyclic",
        .exported_type_declarations = {
            {
                .kind = orison::semantics::PackageTypeDeclarationKind::type_alias,
                .name = "First",
                .aliased_type = orison::syntax::TypeSyntax {.name = "Second"},
                .aliased_type_identity = orison::semantics::GenericSpecializationTypeIdentity {
                    .package_name = "external.cyclic",
                    .declaration_name = "Second",
                },
            },
            {
                .kind = orison::semantics::PackageTypeDeclarationKind::type_alias,
                .name = "Second",
                .aliased_type = orison::syntax::TypeSyntax {.name = "First"},
                .aliased_type_identity = orison::semantics::GenericSpecializationTypeIdentity {
                    .package_name = "external.cyclic",
                    .declaration_name = "First",
                },
            },
        },
    };
    auto cyclic_consumer = orison::syntax::ModuleSyntax {};
    cyclic_consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "First",
        .from_package = "external.cyclic",
    });
    auto cyclic_imported_declarations = orison::semantics::resolve_imported_type_declarations(
        cyclic_consumer,
        {cyclic_interface}
    );
    assert(cyclic_imported_declarations.size() == 1);
    assert(!cyclic_imported_declarations.front().lowering_alias_target.has_value());
    auto cyclic_aggregates = orison::semantics::resolve_imported_aggregate_declarations(
        cyclic_consumer,
        {cyclic_interface}
    );
    assert(cyclic_aggregates.records.empty());
    assert(cyclic_aggregates.choices.empty());

    auto imported_functions = orison::semantics::resolve_imported_function_declarations(
        consumer,
        available_interfaces
    );
    assert(imported_functions.size() == 1);
    assert(imported_functions.front().package_name == "external.types");
    assert(imported_functions.front().declaration_name == "transform");
    assert(imported_functions.front().local_name == "external_transform");
    assert(imported_functions.front().declaration.generic_parameters == std::vector<std::string>({"Value"}));
    assert(imported_functions.front().declaration.parameters.size() == 1);
    assert(imported_functions.front().declaration.return_type.name == "ExternalResult");
    assert(imported_functions.front().declaration.return_type.generic_arguments.front().name == "Value");
    assert(imported_functions.front().declaration.is_async);
    assert(imported_functions.front().declaration.is_unsafe);
    assert(imported_functions.front().declaration.where_constraints.size() == 1);

    auto analysis = orison::semantics::ModuleSemanticAnalyzer {}.analyze(
        consumer,
        orison::semantics::SemanticAnalysisOptions {
            .imported_type_declarations = imported_declarations,
        }
    );
    assert(analysis.has_errors());
    assert(
        analysis.render("consumer.or").find(
            "type 'ExternalPair<UInt32>' supplies 1 type argument; type 'ExternalPair' requires 2"
        ) != std::string::npos
    );
}
