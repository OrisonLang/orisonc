#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/semantics/package_interface.hpp"

#include <algorithm>
#include <cassert>
#include <string>

int main() {
    auto producer = orison::syntax::ModuleSyntax {};
    producer.package_name = "external.types";
    producer.type_aliases.push_back(orison::syntax::TypeAliasSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .name = "Identifier",
        .aliased_type = {.name = "UInt64"},
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
                .payloads = {{.name = "reason", .type = {.name = "Text"}}},
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
            .name = "Maybe",
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

    auto package_interface = orison::semantics::build_package_interface(producer);
    assert(package_interface.package_name == "external.types");
    assert(package_interface.exported_type_declarations.size() == 4);

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
    auto pair = find_export("Pair");
    assert(pair != package_interface.exported_type_declarations.end());
    assert(pair->kind == orison::semantics::PackageTypeDeclarationKind::record);
    assert(pair->generic_arity == 2);
    assert(pair->generic_parameters == std::vector<std::string>({"Left", "Right"}));
    assert(pair->public_record_fields.size() == 1);
    assert(pair->public_record_fields.front().name == "first");
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

    auto serialized = orison::semantics::serialize_package_interface(package_interface);
    assert(
        serialized ==
        "orison-package-interface 2\n"
        "package external.types\n"
        "type alias Identifier 0\n"
        "alias Identifier UInt64\n"
        "type record Pair 2 Left Right\n"
        "field Pair first Left\n"
        "type interface Reader 0\n"
        "type choice Result 1 Value\n"
        "variant Result Ok 1 value Value\n"
        "variant Result Error 1 reason Text\n"
        "function transform 1 Value 1 values shared.View<Value> Maybe<Value> async unsafe 1 Value 2 Reader Shareable\n"
    );
    auto parsed = orison::semantics::parse_package_interface(serialized);
    assert(!parsed.has_error());
    assert(parsed.package_interface.has_value());
    assert(orison::semantics::serialize_package_interface(*parsed.package_interface) == serialized);
    assert(parsed.package_interface->exported_functions.size() == 1);
    assert(parsed.package_interface->exported_functions.front().is_async);
    assert(parsed.package_interface->exported_functions.front().is_unsafe);
    assert(parsed.package_interface->exported_functions.front().where_constraints.size() == 1);

    auto empty = orison::semantics::parse_package_interface("");
    assert(empty.has_error());
    assert(empty.error_message == "package interface is empty");
    auto unsupported_version = orison::semantics::parse_package_interface(
        "orison-package-interface 3\npackage external.types\n"
    );
    assert(unsupported_version.has_error());
    assert(unsupported_version.error_message == "unsupported package interface version '3'");
    auto unknown_kind = orison::semantics::parse_package_interface(
        "orison-package-interface 2\npackage external.types\ntype unknown Pair 2 Left Right\n"
    );
    assert(unknown_kind.has_error());
    assert(unknown_kind.error_message == "unknown package interface type kind 'unknown' at line 3");
    auto invalid_arity = orison::semantics::parse_package_interface(
        "orison-package-interface 2\npackage external.types\ntype record Pair two\n"
    );
    assert(invalid_arity.has_error());
    assert(invalid_arity.error_message == "invalid package interface generic arity 'two' at line 3");
    auto duplicate = orison::semantics::parse_package_interface(
        "orison-package-interface 2\n"
        "package external.types\n"
        "type record Pair 2 Left Right\n"
        "type choice Pair 1 Value\n"
    );
    assert(duplicate.has_error());
    assert(duplicate.error_message == "duplicate package interface type declaration 'Pair' at line 4");
    auto invalid_field = orison::semantics::parse_package_interface(
        "orison-package-interface 2\n"
        "package external.types\n"
        "type record Pair 2 Left Right\n"
        "field Pair first View<\n"
    );
    assert(invalid_field.has_error());
    assert(invalid_field.error_message == "invalid package interface field declaration at line 4");
    auto missing_alias_target = orison::semantics::parse_package_interface(
        "orison-package-interface 2\npackage external.types\ntype alias Identifier 0\n"
    );
    assert(missing_alias_target.has_error());
    assert(missing_alias_target.error_message == "package interface alias 'Identifier' is missing its target");
    auto invalid_function = orison::semantics::parse_package_interface(
        "orison-package-interface 2\n"
        "package external.types\n"
        "function transform 0 1 value UInt32 UInt32 sometimes safe 0\n"
    );
    assert(invalid_function.has_error());
    assert(invalid_function.error_message == "invalid package interface function declaration at line 3");

    auto consumer = orison::syntax::ModuleSyntax {};
    consumer.package_name = "demo.consumer";
    consumer.imports.push_back(orison::syntax::ImportSyntax {
        .name = "Pair",
        .alias = "ExternalPair",
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

    auto imported_declarations = orison::semantics::resolve_imported_type_declarations(
        consumer,
        {package_interface}
    );
    assert(imported_declarations.size() == 1);
    assert(imported_declarations.front().package_name == "external.types");
    assert(imported_declarations.front().declaration_name == "Pair");
    assert(imported_declarations.front().generic_arity == 2);

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
