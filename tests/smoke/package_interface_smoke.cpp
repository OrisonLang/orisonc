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
    });
    producer.records.push_back(orison::syntax::RecordSyntax {.name = "InternalRecord"});
    producer.choices.push_back(orison::syntax::ChoiceSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .name = "Result",
        .generic_parameters = {"Value"},
    });
    producer.interfaces.push_back(orison::syntax::InterfaceSyntax {
        .visibility = orison::syntax::Visibility::public_visibility,
        .name = "Reader",
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
    auto result = find_export("Result");
    assert(result != package_interface.exported_type_declarations.end());
    assert(result->kind == orison::semantics::PackageTypeDeclarationKind::choice);
    assert(result->generic_arity == 1);
    auto reader = find_export("Reader");
    assert(reader != package_interface.exported_type_declarations.end());
    assert(reader->kind == orison::semantics::PackageTypeDeclarationKind::interface_declaration);
    assert(reader->generic_arity == 0);
    assert(find_export("InternalIdentifier") == package_interface.exported_type_declarations.end());
    assert(find_export("InternalRecord") == package_interface.exported_type_declarations.end());

    auto serialized = orison::semantics::serialize_package_interface(package_interface);
    assert(
        serialized ==
        "orison-package-interface 1\n"
        "package external.types\n"
        "type alias Identifier 0\n"
        "type record Pair 2\n"
        "type interface Reader 0\n"
        "type choice Result 1\n"
    );
    auto parsed = orison::semantics::parse_package_interface(serialized);
    assert(!parsed.has_error());
    assert(parsed.package_interface.has_value());
    assert(orison::semantics::serialize_package_interface(*parsed.package_interface) == serialized);

    auto empty = orison::semantics::parse_package_interface("");
    assert(empty.has_error());
    assert(empty.error_message == "package interface is empty");
    auto unsupported_version = orison::semantics::parse_package_interface(
        "orison-package-interface 2\npackage external.types\n"
    );
    assert(unsupported_version.has_error());
    assert(unsupported_version.error_message == "unsupported package interface version '2'");
    auto unknown_kind = orison::semantics::parse_package_interface(
        "orison-package-interface 1\npackage external.types\ntype unknown Pair 2\n"
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
        "type record Pair 2\n"
        "type choice Pair 1\n"
    );
    assert(duplicate.has_error());
    assert(duplicate.error_message == "duplicate package interface type declaration 'Pair' at line 4");

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
