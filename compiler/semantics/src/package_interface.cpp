#include "orison/semantics/package_interface.hpp"

#include <algorithm>
#include <charconv>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace orison::semantics {
namespace {

constexpr auto package_interface_format_version = std::size_t {2};

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

auto render_type(syntax::TypeSyntax const& type) -> std::string {
    auto rendered = type.name;
    if (type.generic_arguments.empty()) {
        return rendered;
    }
    rendered += '<';
    for (auto index = std::size_t {0}; index < type.generic_arguments.size(); ++index) {
        if (index > 0) {
            rendered += ',';
        }
        rendered += render_type(type.generic_arguments[index]);
    }
    rendered += '>';
    return rendered;
}

auto parse_type_at(std::string_view text, std::size_t& offset) -> std::optional<syntax::TypeSyntax> {
    auto start = offset;
    while (offset < text.size() && text[offset] != '<' && text[offset] != ',' && text[offset] != '>') {
        ++offset;
    }
    if (start == offset) {
        return std::nullopt;
    }

    auto type = syntax::TypeSyntax {.name = std::string(text.substr(start, offset - start))};
    if (offset == text.size() || text[offset] != '<') {
        return type;
    }

    ++offset;
    while (true) {
        auto argument = parse_type_at(text, offset);
        if (!argument.has_value()) {
            return std::nullopt;
        }
        type.generic_arguments.push_back(std::move(*argument));
        if (offset >= text.size()) {
            return std::nullopt;
        }
        if (text[offset] == '>') {
            ++offset;
            return type;
        }
        if (text[offset] != ',') {
            return std::nullopt;
        }
        ++offset;
    }
}

auto parse_type(std::string_view text) -> std::optional<syntax::TypeSyntax> {
    auto offset = std::size_t {0};
    auto type = parse_type_at(text, offset);
    if (!type.has_value() || offset != text.size()) {
        return std::nullopt;
    }
    return type;
}

auto parse_count(std::string_view text) -> std::optional<std::size_t> {
    auto value = std::size_t {};
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc {} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

auto find_type_declaration(
    PackageInterface& package_interface,
    std::string_view name,
    PackageTypeDeclarationKind kind
) -> PackageTypeDeclaration* {
    auto found = std::ranges::find_if(
        package_interface.exported_type_declarations,
        [&](PackageTypeDeclaration const& declaration) {
            return declaration.name == name && declaration.kind == kind;
        }
    );
    return found == package_interface.exported_type_declarations.end() ? nullptr : &*found;
}

}  // namespace

auto build_package_interface(syntax::ModuleSyntax const& module) -> PackageInterface {
    auto package_interface = PackageInterface {.package_name = module.package_name};

    for (auto const& type_alias : module.type_aliases) {
        if (type_alias.visibility == syntax::Visibility::public_visibility) {
            package_interface.exported_type_declarations.push_back(PackageTypeDeclaration {
                .kind = PackageTypeDeclarationKind::type_alias,
                .name = type_alias.name,
                .aliased_type = type_alias.aliased_type,
            });
        }
    }
    for (auto const& record : module.records) {
        if (record.visibility == syntax::Visibility::public_visibility) {
            package_interface.exported_type_declarations.push_back(PackageTypeDeclaration {
                .kind = PackageTypeDeclarationKind::record,
                .name = record.name,
                .generic_arity = record.generic_parameters.size(),
                .generic_parameters = record.generic_parameters,
            });
            auto& exported_record = package_interface.exported_type_declarations.back();
            std::ranges::copy_if(
                record.fields,
                std::back_inserter(exported_record.public_record_fields),
                [](syntax::FieldSyntax const& field) {
                    return field.visibility == syntax::Visibility::public_visibility;
                }
            );
        }
    }
    for (auto const& choice : module.choices) {
        if (choice.visibility == syntax::Visibility::public_visibility) {
            package_interface.exported_type_declarations.push_back(PackageTypeDeclaration {
                .kind = PackageTypeDeclarationKind::choice,
                .name = choice.name,
                .generic_arity = choice.generic_parameters.size(),
                .generic_parameters = choice.generic_parameters,
                .choice_variants = choice.variants,
            });
        }
    }
    for (auto const& interface_declaration : module.interfaces) {
        if (interface_declaration.visibility == syntax::Visibility::public_visibility) {
            package_interface.exported_type_declarations.push_back(PackageTypeDeclaration {
                .kind = PackageTypeDeclarationKind::interface_declaration,
                .name = interface_declaration.name,
                .generic_arity = interface_declaration.generic_parameters.size(),
                .generic_parameters = interface_declaration.generic_parameters,
            });
        }
    }

    for (auto const& function : module.functions) {
        if (function.visibility != syntax::Visibility::public_visibility) {
            continue;
        }
        package_interface.exported_functions.push_back(PackageFunctionDeclaration {
            .name = function.name,
            .generic_parameters = function.generic_parameters,
            .parameters = function.parameters,
            .return_type = function.return_type,
            .where_constraints = function.where_constraints,
            .is_async = function.is_async,
            .is_unsafe = function.is_unsafe,
        });
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
    auto functions = package_interface.exported_functions;
    std::ranges::sort(functions, {}, &PackageFunctionDeclaration::name);

    auto output = std::ostringstream {};
    output << "orison-package-interface " << package_interface_format_version << '\n';
    output << "package " << package_interface.package_name << '\n';
    for (auto const& declaration : declarations) {
        output << "type " << declaration_kind_text(declaration.kind) << ' ' << declaration.name << ' '
               << declaration.generic_parameters.size();
        for (auto const& generic_parameter : declaration.generic_parameters) {
            output << ' ' << generic_parameter;
        }
        output << '\n';
        if (declaration.aliased_type.has_value()) {
            output << "alias " << declaration.name << ' ' << render_type(*declaration.aliased_type) << '\n';
        }
        for (auto const& field : declaration.public_record_fields) {
            output << "field " << declaration.name << ' ' << field.name << ' ' << render_type(field.type) << '\n';
        }
        for (auto const& variant : declaration.choice_variants) {
            output << "variant " << declaration.name << ' ' << variant.name << ' ' << variant.payloads.size();
            for (auto const& payload : variant.payloads) {
                output << ' ' << payload.name << ' ' << render_type(payload.type);
            }
            output << '\n';
        }
    }
    for (auto const& function : functions) {
        output << "function " << function.name << ' ' << function.generic_parameters.size();
        for (auto const& generic_parameter : function.generic_parameters) {
            output << ' ' << generic_parameter;
        }
        output << ' ' << function.parameters.size();
        for (auto const& parameter : function.parameters) {
            output << ' ' << parameter.name << ' ' << render_type(parameter.type);
        }
        output << ' ' << render_type(function.return_type)
               << ' ' << (function.is_async ? "async" : "sync")
               << ' ' << (function.is_unsafe ? "unsafe" : "safe")
               << ' ' << function.where_constraints.size();
        for (auto const& constraint : function.where_constraints) {
            output << ' ' << constraint.parameter_name << ' ' << constraint.requirements.size();
            for (auto const& requirement : constraint.requirements) {
                output << ' ' << render_type(requirement);
            }
        }
        output << '\n';
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
    auto exported_function_names = std::unordered_set<std::string> {};
    auto line_number = std::size_t {2};
    while (std::getline(input, line)) {
        ++line_number;
        remove_carriage_return(line);
        auto declaration_line = std::istringstream {line};
        auto record_kind = std::string {};
        declaration_line >> record_kind;
        if (record_kind == "type") {
            auto kind_text = std::string {};
            auto declaration_name = std::string {};
            auto arity_text = std::string {};
            if (!(declaration_line >> kind_text >> declaration_name >> arity_text)) {
                return parse_error("invalid package interface type declaration at line " + std::to_string(line_number));
            }

            auto kind = declaration_kind_from_text(kind_text);
            if (!kind.has_value()) {
                return parse_error(
                    "unknown package interface type kind '" + kind_text + "' at line " + std::to_string(line_number)
                );
            }
            auto generic_arity = parse_count(arity_text);
            if (!generic_arity.has_value()) {
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

            auto declaration = PackageTypeDeclaration {
                .kind = *kind,
                .name = std::move(declaration_name),
                .generic_arity = *generic_arity,
            };
            for (auto index = std::size_t {0}; index < *generic_arity; ++index) {
                auto generic_parameter = std::string {};
                if (!(declaration_line >> generic_parameter)) {
                    return parse_error(
                        "invalid package interface type declaration at line " + std::to_string(line_number)
                    );
                }
                declaration.generic_parameters.push_back(std::move(generic_parameter));
            }
            if (declaration_line >> extra) {
                return parse_error("invalid package interface type declaration at line " + std::to_string(line_number));
            }
            result.exported_type_declarations.push_back(std::move(declaration));
            continue;
        }

        if (record_kind == "alias") {
            auto owner_name = std::string {};
            auto type_text = std::string {};
            if (!(declaration_line >> owner_name >> type_text) || declaration_line >> extra) {
                return parse_error("invalid package interface alias declaration at line " + std::to_string(line_number));
            }
            auto* owner = find_type_declaration(result, owner_name, PackageTypeDeclarationKind::type_alias);
            auto target = parse_type(type_text);
            if (owner == nullptr || owner->aliased_type.has_value() || !target.has_value()) {
                return parse_error("invalid package interface alias declaration at line " + std::to_string(line_number));
            }
            owner->aliased_type = std::move(*target);
            continue;
        }

        if (record_kind == "field") {
            auto owner_name = std::string {};
            auto field_name = std::string {};
            auto type_text = std::string {};
            if (!(declaration_line >> owner_name >> field_name >> type_text) || declaration_line >> extra) {
                return parse_error("invalid package interface field declaration at line " + std::to_string(line_number));
            }
            auto* owner = find_type_declaration(result, owner_name, PackageTypeDeclarationKind::record);
            auto field_type = parse_type(type_text);
            if (owner == nullptr || !field_type.has_value() || std::ranges::any_of(
                    owner->public_record_fields,
                    [&](syntax::FieldSyntax const& field) { return field.name == field_name; }
                )) {
                return parse_error("invalid package interface field declaration at line " + std::to_string(line_number));
            }
            owner->public_record_fields.push_back(syntax::FieldSyntax {
                .visibility = syntax::Visibility::public_visibility,
                .name = std::move(field_name),
                .type = std::move(*field_type),
            });
            continue;
        }

        if (record_kind == "variant") {
            auto owner_name = std::string {};
            auto variant_name = std::string {};
            auto payload_count_text = std::string {};
            if (!(declaration_line >> owner_name >> variant_name >> payload_count_text)) {
                return parse_error("invalid package interface variant declaration at line " + std::to_string(line_number));
            }
            auto* owner = find_type_declaration(result, owner_name, PackageTypeDeclarationKind::choice);
            auto payload_count = parse_count(payload_count_text);
            if (owner == nullptr || !payload_count.has_value() || std::ranges::any_of(
                    owner->choice_variants,
                    [&](syntax::ChoiceVariantSyntax const& variant) { return variant.name == variant_name; }
                )) {
                return parse_error("invalid package interface variant declaration at line " + std::to_string(line_number));
            }
            auto variant = syntax::ChoiceVariantSyntax {.name = std::move(variant_name)};
            for (auto index = std::size_t {0}; index < *payload_count; ++index) {
                auto payload_name = std::string {};
                auto payload_type_text = std::string {};
                if (!(declaration_line >> payload_name >> payload_type_text)) {
                    return parse_error("invalid package interface variant declaration at line " + std::to_string(line_number));
                }
                auto payload_type = parse_type(payload_type_text);
                if (!payload_type.has_value()) {
                    return parse_error("invalid package interface variant declaration at line " + std::to_string(line_number));
                }
                variant.payloads.push_back(syntax::NamedTypeSyntax {
                    .name = std::move(payload_name),
                    .type = std::move(*payload_type),
                });
            }
            if (declaration_line >> extra) {
                return parse_error("invalid package interface variant declaration at line " + std::to_string(line_number));
            }
            owner->choice_variants.push_back(std::move(variant));
            continue;
        }

        if (record_kind == "function") {
            auto function = PackageFunctionDeclaration {};
            auto generic_count_text = std::string {};
            if (!(declaration_line >> function.name >> generic_count_text)) {
                return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
            }
            auto generic_count = parse_count(generic_count_text);
            if (!generic_count.has_value() || !exported_function_names.insert(function.name).second) {
                return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
            }
            for (auto index = std::size_t {0}; index < *generic_count; ++index) {
                auto generic_parameter = std::string {};
                if (!(declaration_line >> generic_parameter)) {
                    return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
                }
                function.generic_parameters.push_back(std::move(generic_parameter));
            }

            auto parameter_count_text = std::string {};
            if (!(declaration_line >> parameter_count_text)) {
                return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
            }
            auto parameter_count = parse_count(parameter_count_text);
            if (!parameter_count.has_value()) {
                return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
            }
            for (auto index = std::size_t {0}; index < *parameter_count; ++index) {
                auto parameter_name = std::string {};
                auto parameter_type_text = std::string {};
                if (!(declaration_line >> parameter_name >> parameter_type_text)) {
                    return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
                }
                auto parameter_type = parse_type(parameter_type_text);
                if (!parameter_type.has_value()) {
                    return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
                }
                function.parameters.push_back(syntax::ParameterSyntax {
                    .name = std::move(parameter_name),
                    .type = std::move(*parameter_type),
                });
            }

            auto return_type_text = std::string {};
            auto async_text = std::string {};
            auto unsafe_text = std::string {};
            auto constraint_count_text = std::string {};
            if (!(declaration_line >> return_type_text >> async_text >> unsafe_text >> constraint_count_text)) {
                return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
            }
            auto return_type = parse_type(return_type_text);
            auto constraint_count = parse_count(constraint_count_text);
            if (!return_type.has_value() || !constraint_count.has_value() ||
                (async_text != "async" && async_text != "sync") ||
                (unsafe_text != "unsafe" && unsafe_text != "safe")) {
                return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
            }
            function.return_type = std::move(*return_type);
            function.is_async = async_text == "async";
            function.is_unsafe = unsafe_text == "unsafe";
            for (auto index = std::size_t {0}; index < *constraint_count; ++index) {
                auto constraint = syntax::WhereConstraintSyntax {};
                auto requirement_count_text = std::string {};
                if (!(declaration_line >> constraint.parameter_name >> requirement_count_text)) {
                    return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
                }
                auto requirement_count = parse_count(requirement_count_text);
                if (!requirement_count.has_value()) {
                    return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
                }
                for (auto requirement_index = std::size_t {0}; requirement_index < *requirement_count;
                     ++requirement_index) {
                    auto requirement_text = std::string {};
                    if (!(declaration_line >> requirement_text)) {
                        return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
                    }
                    auto requirement = parse_type(requirement_text);
                    if (!requirement.has_value()) {
                        return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
                    }
                    constraint.requirements.push_back(std::move(*requirement));
                }
                function.where_constraints.push_back(std::move(constraint));
            }
            if (declaration_line >> extra) {
                return parse_error("invalid package interface function declaration at line " + std::to_string(line_number));
            }
            result.exported_functions.push_back(std::move(function));
            continue;
        }

        return parse_error("unknown package interface record '" + record_kind + "' at line " + std::to_string(line_number));
    }

    for (auto const& declaration : result.exported_type_declarations) {
        if (declaration.kind == PackageTypeDeclarationKind::type_alias && !declaration.aliased_type.has_value()) {
            return parse_error("package interface alias '" + declaration.name + "' is missing its target");
        }
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
