#include "orison/semantics/package_interface.hpp"

#include <algorithm>

namespace orison::semantics {

auto build_package_interface(syntax::ModuleSyntax const& module) -> PackageInterface {
    auto package_interface = PackageInterface {.package_name = module.package_name};

    for (auto const& type_alias : module.type_aliases) {
        if (type_alias.visibility == syntax::Visibility::public_visibility) {
            package_interface.exported_type_declarations.push_back(PackageTypeDeclaration {
                .kind = PackageTypeDeclarationKind::type_alias,
                .name = type_alias.name,
            });
        }
    }
    for (auto const& record : module.records) {
        if (record.visibility == syntax::Visibility::public_visibility) {
            package_interface.exported_type_declarations.push_back(PackageTypeDeclaration {
                .kind = PackageTypeDeclarationKind::record,
                .name = record.name,
                .generic_arity = record.generic_parameters.size(),
            });
        }
    }
    for (auto const& choice : module.choices) {
        if (choice.visibility == syntax::Visibility::public_visibility) {
            package_interface.exported_type_declarations.push_back(PackageTypeDeclaration {
                .kind = PackageTypeDeclarationKind::choice,
                .name = choice.name,
                .generic_arity = choice.generic_parameters.size(),
            });
        }
    }
    for (auto const& interface_declaration : module.interfaces) {
        if (interface_declaration.visibility == syntax::Visibility::public_visibility) {
            package_interface.exported_type_declarations.push_back(PackageTypeDeclaration {
                .kind = PackageTypeDeclarationKind::interface_declaration,
                .name = interface_declaration.name,
                .generic_arity = interface_declaration.generic_parameters.size(),
            });
        }
    }

    return package_interface;
}

auto resolve_imported_type_declarations(
    syntax::ModuleSyntax const& module,
    std::vector<PackageInterface> const& package_interfaces
) -> std::vector<ImportedTypeDeclarationMetadata> {
    auto imported_declarations = std::vector<ImportedTypeDeclarationMetadata> {};

    for (auto const& import : module.imports) {
        for (auto const& package_interface : package_interfaces) {
            if (package_interface.package_name != import.from_package) {
                continue;
            }

            auto declaration = std::ranges::find_if(
                package_interface.exported_type_declarations,
                [&](PackageTypeDeclaration const& candidate) {
                    return candidate.name == import.name;
                }
            );
            if (declaration == package_interface.exported_type_declarations.end()) {
                continue;
            }

            imported_declarations.push_back(ImportedTypeDeclarationMetadata {
                .package_name = package_interface.package_name,
                .declaration_name = declaration->name,
                .generic_arity = declaration->generic_arity,
            });
            break;
        }
    }

    return imported_declarations;
}

}  // namespace orison::semantics
