#pragma once

#include "orison/syntax/module_parser.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace orison::semantics {

enum class PackageTypeDeclarationKind {
    type_alias,
    record,
    choice,
    interface_declaration,
};

struct PackageTypeDeclaration {
    PackageTypeDeclarationKind kind = PackageTypeDeclarationKind::type_alias;
    std::string name;
    std::size_t generic_arity = 0;
    std::vector<std::string> generic_parameters;
    std::optional<syntax::TypeSyntax> aliased_type;
    std::vector<syntax::FieldSyntax> public_record_fields;
    std::vector<syntax::ChoiceVariantSyntax> choice_variants;
};

struct PackageFunctionDeclaration {
    std::string name;
    std::vector<std::string> generic_parameters;
    std::vector<syntax::ParameterSyntax> parameters;
    syntax::TypeSyntax return_type;
    std::vector<syntax::WhereConstraintSyntax> where_constraints;
    bool is_async = false;
    bool is_unsafe = false;
};

struct PackageInterface {
    std::string package_name;
    std::vector<PackageTypeDeclaration> exported_type_declarations;
    std::vector<PackageFunctionDeclaration> exported_functions;
};

struct PackageInterfaceParseResult {
    std::optional<PackageInterface> package_interface;
    std::string error_message;

    auto has_error() const -> bool {
        return !error_message.empty();
    }
};

struct ImportedTypeDeclarationMetadata {
    std::string package_name;
    std::string declaration_name;
    std::size_t generic_arity = 0;
};

struct ImportedAggregateDeclarations {
    std::vector<syntax::RecordSyntax> records;
    std::vector<syntax::ChoiceSyntax> choices;
};

auto build_package_interface(syntax::ModuleSyntax const& module) -> PackageInterface;

auto serialize_package_interface(PackageInterface const& package_interface) -> std::string;

auto parse_package_interface(std::string_view text) -> PackageInterfaceParseResult;

auto resolve_imported_type_declarations(
    syntax::ModuleSyntax const& module,
    std::vector<PackageInterface> const& package_interfaces
) -> std::vector<ImportedTypeDeclarationMetadata>;

auto resolve_imported_aggregate_declarations(
    syntax::ModuleSyntax const& module,
    std::vector<PackageInterface> const& package_interfaces
) -> ImportedAggregateDeclarations;

}  // namespace orison::semantics
