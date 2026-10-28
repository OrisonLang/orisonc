#include "orison/semantics/package_interface.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace orison::semantics {
namespace {

constexpr auto package_interface_format_version = std::size_t {1};

auto declaration_kind_text(PackageTypeDeclarationKind kind) -> std::string_view {
    switch (kind) {
    case PackageTypeDeclarationKind::type_alias:
        return "alias";
    case PackageTypeDeclarationKind::record:
        return "record";
    case PackageTypeDeclarationKind::choice:
        return "choice";
    case PackageTypeDeclarationKind::interface_declaration:
        return "interface";
    }
    return "alias";
}

auto declaration_kind_from_text(std::string_view text) -> std::optional<PackageTypeDeclarationKind> {
    if (text == "alias") {
        return PackageTypeDeclarationKind::type_alias;
    }
    if (text == "record") {
        return PackageTypeDeclarationKind::record;
    }
    if (text == "choice") {
        return PackageTypeDeclarationKind::choice;
    }
    if (text == "interface") {
        return PackageTypeDeclarationKind::interface_declaration;
    }
    return std::nullopt;
}

auto parse_error(std::string message) -> PackageInterfaceParseResult {
    return PackageInterfaceParseResult {.error_message = std::move(message)};
}

void remove_carriage_return(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

}  // namespace

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

auto serialize_package_interface(PackageInterface const& package_interface) -> std::string {
    auto declarations = package_interface.exported_type_declarations;
    std::ranges::sort(
        declarations,
        [](PackageTypeDeclaration const& left, PackageTypeDeclaration const& right) {
            if (left.name != right.name) {
                return left.name < right.name;
            }
            if (left.kind != right.kind) {
                return left.kind < right.kind;
            }
            return left.generic_arity < right.generic_arity;
        }
    );

    auto output = std::ostringstream {};
    output << "orison-package-interface " << package_interface_format_version << '\n';
    output << "package " << package_interface.package_name << '\n';
    for (auto const& declaration : declarations) {
        output << "type " << declaration_kind_text(declaration.kind) << ' ' << declaration.name << ' '
               << declaration.generic_arity << '\n';
    }
    return output.str();
}

auto parse_package_interface(std::string_view text) -> PackageInterfaceParseResult {
    auto input = std::istringstream {std::string(text)};
    auto line = std::string {};
    if (!std::getline(input, line)) {
        return parse_error("package interface is empty");
    }
    remove_carriage_return(line);

    auto header = std::istringstream {line};
    auto marker = std::string {};
    auto version = std::size_t {};
    auto extra = std::string {};
    if (!(header >> marker >> version) || marker != "orison-package-interface" || header >> extra) {
        return parse_error("invalid package interface header");
    }
    if (version != package_interface_format_version) {
        return parse_error("unsupported package interface version '" + std::to_string(version) + "'");
    }

    if (!std::getline(input, line)) {
        return parse_error("package interface is missing its package declaration");
    }
    remove_carriage_return(line);
    auto package_line = std::istringstream {line};
    auto package_keyword = std::string {};
    auto package_name = std::string {};
    if (!(package_line >> package_keyword >> package_name) || package_keyword != "package" || package_line >> extra) {
        return parse_error("invalid package interface package declaration at line 2");
    }

    auto result = PackageInterface {.package_name = std::move(package_name)};
    auto exported_names = std::unordered_set<std::string> {};
    auto line_number = std::size_t {2};
    while (std::getline(input, line)) {
        ++line_number;
        remove_carriage_return(line);
        auto declaration_line = std::istringstream {line};
        auto type_keyword = std::string {};
        auto kind_text = std::string {};
        auto declaration_name = std::string {};
        auto arity_text = std::string {};
        if (!(declaration_line >> type_keyword >> kind_text >> declaration_name >> arity_text) ||
            type_keyword != "type" || declaration_line >> extra) {
            return parse_error("invalid package interface type declaration at line " + std::to_string(line_number));
        }

        auto kind = declaration_kind_from_text(kind_text);
        if (!kind.has_value()) {
            return parse_error(
                "unknown package interface type kind '" + kind_text + "' at line " + std::to_string(line_number)
            );
        }

        auto generic_arity = std::size_t {};
        auto [arity_end, arity_error] = std::from_chars(
            arity_text.data(),
            arity_text.data() + arity_text.size(),
            generic_arity
        );
        if (arity_error != std::errc {} || arity_end != arity_text.data() + arity_text.size()) {
            return parse_error(
                "invalid package interface generic arity '" + arity_text + "' at line " +
                std::to_string(line_number)
            );
        }
        if (!exported_names.insert(declaration_name).second) {
            return parse_error(
                "duplicate package interface type declaration '" + declaration_name + "' at line " +
                std::to_string(line_number)
            );
        }

        result.exported_type_declarations.push_back(PackageTypeDeclaration {
            .kind = *kind,
            .name = std::move(declaration_name),
            .generic_arity = generic_arity,
        });
    }

    return PackageInterfaceParseResult {.package_interface = std::move(result)};
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
