#include "orison/semantics/package_interface.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace orison::semantics {
namespace {

constexpr auto package_interface_format_version = std::size_t {1};
constexpr auto generic_specialization_request_format_version = std::size_t {1};

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

auto specialization_request_parse_error(std::string message)
    -> GenericFunctionSpecializationRequestParseResult {
    return GenericFunctionSpecializationRequestParseResult {.error_message = std::move(message)};
}

void remove_carriage_return(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

auto trimmed_text(std::string_view text) -> std::string_view {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    return text;
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

auto render_constant_expression(syntax::ExpressionSyntax const& expression) -> std::string {
    using syntax::ExpressionKind;
    switch (expression.kind) {
    case ExpressionKind::name: {
        auto rendered = expression.text;
        if (!expression.generic_arguments.empty()) {
            rendered += "<";
            for (auto index = std::size_t {0}; index < expression.generic_arguments.size(); ++index) {
                if (index != 0) {
                    rendered += ", ";
                }
                rendered += render_type(expression.generic_arguments[index]);
            }
            rendered += ">";
        }
        return rendered;
    }
    case ExpressionKind::integer_literal:
    case ExpressionKind::float_literal:
    case ExpressionKind::string_literal:
    case ExpressionKind::boolean_literal:
        return expression.text;
    case ExpressionKind::array_literal: {
        auto rendered = std::string {"["};
        for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
            if (index != 0) {
                rendered += ", ";
            }
            rendered += render_constant_expression(expression.arguments[index]);
        }
        rendered += "]";
        return rendered;
    }
    case ExpressionKind::unary:
        if (expression.left == nullptr) {
            return {};
        }
        if (expression.text == "not" || expression.text == "bit_not") {
            return expression.text + " " + render_constant_expression(*expression.left);
        }
        return expression.text + render_constant_expression(*expression.left);
    case ExpressionKind::cast:
        return expression.left == nullptr
            ? std::string {}
            : render_constant_expression(*expression.left) + " as " + expression.text;
    case ExpressionKind::call: {
        if (expression.left == nullptr) {
            return {};
        }
        auto rendered = render_constant_expression(*expression.left) + "(";
        for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
            if (index != 0) {
                rendered += ", ";
            }
            rendered += render_constant_expression(expression.arguments[index]);
        }
        rendered += ")";
        return rendered;
    }
    case ExpressionKind::member_access:
        return expression.left == nullptr
            ? std::string {}
            : render_constant_expression(*expression.left) + "." + expression.text;
    case ExpressionKind::null_safe_member_access:
        return expression.left == nullptr
            ? std::string {}
            : render_constant_expression(*expression.left) + "?." + expression.text;
    case ExpressionKind::index_access:
        return expression.left == nullptr || expression.arguments.size() != 1
            ? std::string {}
            : render_constant_expression(*expression.left) + "[" +
                render_constant_expression(expression.arguments.front()) + "]";
    case ExpressionKind::binary:
        return expression.left == nullptr || expression.right == nullptr
            ? std::string {}
            : "(" + render_constant_expression(*expression.left) + " " + expression.text + " " +
                render_constant_expression(*expression.right) + ")";
    case ExpressionKind::ternary:
        return expression.left == nullptr || expression.right == nullptr || expression.alternate == nullptr
            ? std::string {}
            : "(" + render_constant_expression(*expression.left) + " ? " +
                render_constant_expression(*expression.right) + " : " +
                render_constant_expression(*expression.alternate) + ")";
    case ExpressionKind::task:
    case ExpressionKind::thread:
        return {};
    }
    return {};
}

auto clone_constant_expression(syntax::ExpressionSyntax const& expression) -> syntax::ExpressionSyntax {
    auto cloned = syntax::ExpressionSyntax {
        .kind = expression.kind,
        .line = expression.line,
        .text = expression.text,
        .generic_arguments = expression.generic_arguments,
    };
    for (auto const& argument : expression.arguments) {
        cloned.arguments.push_back(clone_constant_expression(argument));
    }
    if (expression.left != nullptr) {
        cloned.left = std::make_unique<syntax::ExpressionSyntax>(clone_constant_expression(*expression.left));
    }
    if (expression.right != nullptr) {
        cloned.right = std::make_unique<syntax::ExpressionSyntax>(clone_constant_expression(*expression.right));
    }
    if (expression.alternate != nullptr) {
        cloned.alternate = std::make_unique<syntax::ExpressionSyntax>(
            clone_constant_expression(*expression.alternate)
        );
    }
    return cloned;
}

auto expand_constant_expression(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::ConstantSyntax const*> const& constants,
    std::unordered_set<std::string>& active_constants
) -> syntax::ExpressionSyntax {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto constant = constants.find(expression.text);
        if (constant != constants.end() && active_constants.insert(expression.text).second) {
            auto expanded = expand_constant_expression(
                constant->second->initializer,
                constants,
                active_constants
            );
            active_constants.erase(expression.text);
            return expanded;
        }
    }

    auto expanded = syntax::ExpressionSyntax {
        .kind = expression.kind,
        .line = expression.line,
        .text = expression.text,
        .generic_arguments = expression.generic_arguments,
    };
    for (auto const& argument : expression.arguments) {
        expanded.arguments.push_back(expand_constant_expression(argument, constants, active_constants));
    }
    if (expression.left != nullptr) {
        expanded.left = std::make_unique<syntax::ExpressionSyntax>(
            expression.kind == syntax::ExpressionKind::call
                ? clone_constant_expression(*expression.left)
                : expand_constant_expression(*expression.left, constants, active_constants)
        );
    }
    if (expression.right != nullptr) {
        expanded.right = std::make_unique<syntax::ExpressionSyntax>(
            expand_constant_expression(*expression.right, constants, active_constants)
        );
    }
    if (expression.alternate != nullptr) {
        expanded.alternate = std::make_unique<syntax::ExpressionSyntax>(
            expand_constant_expression(*expression.alternate, constants, active_constants)
        );
    }
    return expanded;
}

auto parse_constant_declaration(
    std::string_view name,
    syntax::TypeSyntax const& type,
    std::string_view initializer_text
) -> std::optional<syntax::ConstantSyntax> {
    auto source_text = "package orison.package_interface\nconst " + std::string(name) + ": " +
        render_type(type) + " = " + std::string(initializer_text) + "\n";
    auto source_file = source::SourceFile {"<package-interface>", std::move(source_text)};
    auto parsed = syntax::ModuleParser {}.parse(source_file);
    if (parsed.diagnostics.has_errors() || parsed.module.constants.size() != 1) {
        return std::nullopt;
    }
    return std::move(parsed.module.constants.front());
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

auto default_type_identity(syntax::TypeSyntax const& type) -> GenericSpecializationTypeIdentity {
    auto identity = GenericSpecializationTypeIdentity {.declaration_name = type.name};
    identity.generic_arguments.reserve(type.generic_arguments.size());
    for (auto const& argument : type.generic_arguments) {
        identity.generic_arguments.push_back(default_type_identity(argument));
    }
    return identity;
}

auto package_interface_type_identity(
    syntax::TypeSyntax const& type,
    syntax::ModuleSyntax const& module
) -> GenericSpecializationTypeIdentity {
    auto identity = GenericSpecializationTypeIdentity {.declaration_name = type.name};
    auto imported = std::ranges::find_if(
        module.imports,
        [&](syntax::ImportSyntax const& candidate) {
            auto const local_name = candidate.alias.empty() ? candidate.name : candidate.alias;
            return local_name == type.name;
        }
    );
    if (imported != module.imports.end()) {
        identity.package_name = imported->from_package;
        identity.declaration_name = imported->name;
    } else {
        auto const local_type = std::ranges::any_of(
            module.type_aliases,
            [&](syntax::TypeAliasSyntax const& candidate) { return candidate.name == type.name; }
        ) || std::ranges::any_of(
            module.records,
            [&](syntax::RecordSyntax const& candidate) { return candidate.name == type.name; }
        ) || std::ranges::any_of(
            module.choices,
            [&](syntax::ChoiceSyntax const& candidate) { return candidate.name == type.name; }
        ) || std::ranges::any_of(
            module.interfaces,
            [&](syntax::InterfaceSyntax const& candidate) { return candidate.name == type.name; }
        );
        if (local_type) {
            identity.package_name = module.package_name;
        }
    }
    for (auto const& argument : type.generic_arguments) {
        identity.generic_arguments.push_back(package_interface_type_identity(argument, module));
    }
    return identity;
}

struct ResolvedPackageAggregate {
    PackageTypeDeclaration const* declaration = nullptr;
};

auto type_from_identity(GenericSpecializationTypeIdentity const& identity) -> syntax::TypeSyntax {
    auto type = syntax::TypeSyntax {.name = identity.declaration_name};
    for (auto const& argument : identity.generic_arguments) {
        type.generic_arguments.push_back(type_from_identity(argument));
    }
    return type;
}

auto resolve_package_alias_target(
    GenericSpecializationTypeIdentity const& identity,
    PackageInterface const& containing_package_interface,
    std::vector<PackageInterface> const& package_interfaces,
    std::vector<std::string> alias_path = {}
) -> std::optional<syntax::TypeSyntax> {
    auto const& package_name = identity.package_name.empty()
        ? containing_package_interface.package_name
        : identity.package_name;
    auto package_interface = std::ranges::find_if(
        package_interfaces,
        [&](PackageInterface const& candidate) {
            return candidate.package_name == package_name;
        }
    );
    if (package_interface == package_interfaces.end()) {
        return type_from_identity(identity);
    }
    auto declaration = std::ranges::find_if(
        package_interface->exported_type_declarations,
        [&](PackageTypeDeclaration const& candidate) {
            return candidate.name == identity.declaration_name;
        }
    );
    if (declaration == package_interface->exported_type_declarations.end()) {
        return type_from_identity(identity);
    }
    if (declaration->kind == PackageTypeDeclarationKind::record ||
        declaration->kind == PackageTypeDeclarationKind::choice) {
        if (declaration->name == "Maybe" && !identity.generic_arguments.empty()) {
            return type_from_identity(identity);
        }
        return std::nullopt;
    }
    if (declaration->kind != PackageTypeDeclarationKind::type_alias ||
        !declaration->aliased_type.has_value()) {
        return type_from_identity(identity);
    }
    auto const alias_key = package_interface->package_name + "::" + declaration->name;
    if (std::ranges::find(alias_path, alias_key) != alias_path.end()) {
        return std::nullopt;
    }
    alias_path.push_back(alias_key);
    auto const target_identity = declaration->aliased_type_identity.value_or(
        default_type_identity(*declaration->aliased_type)
    );
    return resolve_package_alias_target(
        target_identity,
        *package_interface,
        package_interfaces,
        std::move(alias_path)
    );
}

auto resolve_package_aggregate(
    GenericSpecializationTypeIdentity const& identity,
    PackageInterface const& containing_package_interface,
    std::vector<PackageInterface> const& package_interfaces,
    std::vector<std::string> alias_path = {}
) -> ResolvedPackageAggregate {
    auto const& package_name = identity.package_name.empty()
        ? containing_package_interface.package_name
        : identity.package_name;
    auto package_interface = std::ranges::find_if(
        package_interfaces,
        [&](PackageInterface const& candidate) {
            return candidate.package_name == package_name;
        }
    );
    if (package_interface == package_interfaces.end()) {
        return {};
    }
    auto declaration = std::ranges::find_if(
        package_interface->exported_type_declarations,
        [&](PackageTypeDeclaration const& candidate) {
            return candidate.name == identity.declaration_name;
        }
    );
    if (declaration == package_interface->exported_type_declarations.end()) {
        return {};
    }
    if (declaration->kind == PackageTypeDeclarationKind::record ||
        declaration->kind == PackageTypeDeclarationKind::choice) {
        return ResolvedPackageAggregate {
            .declaration = &*declaration,
        };
    }
    if (declaration->kind != PackageTypeDeclarationKind::type_alias ||
        !declaration->aliased_type.has_value()) {
        return {};
    }
    auto const alias_key = package_interface->package_name + "::" + declaration->name;
    if (std::ranges::find(alias_path, alias_key) != alias_path.end()) {
        return {};
    }
    alias_path.push_back(alias_key);
    auto const target_identity = declaration->aliased_type_identity.value_or(
        default_type_identity(*declaration->aliased_type)
    );
    return resolve_package_aggregate(
        target_identity,
        *package_interface,
        package_interfaces,
        std::move(alias_path)
    );
}

auto render_type_identity(GenericSpecializationTypeIdentity const& identity) -> std::string {
    auto rendered = identity.package_name.empty()
        ? identity.declaration_name
        : identity.package_name + "::" + identity.declaration_name;
    if (identity.generic_arguments.empty()) {
        return rendered;
    }
    rendered += '<';
    for (auto index = std::size_t {0}; index < identity.generic_arguments.size(); ++index) {
        if (index > 0) {
            rendered += ',';
        }
        rendered += render_type_identity(identity.generic_arguments[index]);
    }
    rendered += '>';
    return rendered;
}

auto canonical_type_from_identity(GenericSpecializationTypeIdentity const& identity) -> syntax::TypeSyntax {
    auto type = syntax::TypeSyntax {.name = identity.declaration_name};
    type.generic_arguments.reserve(identity.generic_arguments.size());
    for (auto const& argument : identity.generic_arguments) {
        type.generic_arguments.push_back(canonical_type_from_identity(argument));
    }
    return type;
}

auto type_identity_from_wire_type(syntax::TypeSyntax const& type)
    -> std::optional<GenericSpecializationTypeIdentity> {
    auto separator = type.name.rfind("::");
    auto identity = GenericSpecializationTypeIdentity {};
    if (separator == std::string::npos) {
        identity.declaration_name = type.name;
    } else {
        identity.package_name = type.name.substr(0, separator);
        identity.declaration_name = type.name.substr(separator + 2);
    }
    if (identity.declaration_name.empty()) {
        return std::nullopt;
    }
    for (auto const& argument : type.generic_arguments) {
        auto converted = type_identity_from_wire_type(argument);
        if (!converted.has_value()) {
            return std::nullopt;
        }
        identity.generic_arguments.push_back(std::move(*converted));
    }
    return identity;
}

auto parse_type_identity(std::string_view text) -> std::optional<GenericSpecializationTypeIdentity> {
    auto wire_type = parse_type(text);
    return wire_type.has_value()
        ? type_identity_from_wire_type(*wire_type)
        : std::nullopt;
}

auto parse_count(std::string_view text) -> std::optional<std::size_t> {
    auto value = std::size_t {};
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc {} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

auto specialization_request_sort_key(GenericFunctionSpecializationRequest const& request) -> std::string {
    auto key = request.package_name + "\n" + request.declaration_name;
    for (auto index = std::size_t {0}; index < request.type_arguments.size(); ++index) {
        auto identity = index < request.type_identities.size()
            ? request.type_identities[index]
            : default_type_identity(request.type_arguments[index]);
        key += "\n" + render_type_identity(identity);
    }
    return key;
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

auto localize_imported_type(
    syntax::TypeSyntax type,
    syntax::ModuleSyntax const& module,
    std::string_view package_name
) -> syntax::TypeSyntax {
    auto imported_type = std::ranges::find_if(
        module.imports,
        [&](syntax::ImportSyntax const& import) {
            return import.from_package == package_name && import.name == type.name;
        }
    );
    if (imported_type != module.imports.end() && !imported_type->alias.empty()) {
        type.name = imported_type->alias;
    }
    for (auto& argument : type.generic_arguments) {
        argument = localize_imported_type(std::move(argument), module, package_name);
    }
    return type;
}

void localize_imported_constant_expression(
    syntax::ExpressionSyntax& expression,
    syntax::ModuleSyntax const& module,
    std::string_view package_name
) {
    for (auto& generic_argument : expression.generic_arguments) {
        generic_argument = localize_imported_type(generic_argument, module, package_name);
    }
    if (expression.kind == syntax::ExpressionKind::call && expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        auto imported_type = std::ranges::find_if(
            module.imports,
            [&](syntax::ImportSyntax const& import) {
                return import.from_package == package_name &&
                    import.name == expression.left->text && !import.alias.empty();
            }
        );
        if (imported_type != module.imports.end()) {
            expression.left->text = imported_type->alias;
        }
    }
    for (auto& argument : expression.arguments) {
        localize_imported_constant_expression(argument, module, package_name);
    }
    if (expression.left != nullptr) {
        localize_imported_constant_expression(*expression.left, module, package_name);
    }
    if (expression.right != nullptr) {
        localize_imported_constant_expression(*expression.right, module, package_name);
    }
    if (expression.alternate != nullptr) {
        localize_imported_constant_expression(*expression.alternate, module, package_name);
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
                .aliased_type = type_alias.aliased_type,
                .aliased_type_identity = package_interface_type_identity(
                    type_alias.aliased_type,
                    module
                ),
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
            for (auto const& field : record.fields) {
                if (field.visibility != syntax::Visibility::public_visibility) {
                    continue;
                }
                exported_record.public_record_fields.push_back(field);
                exported_record.public_record_field_type_identities.push_back(
                    package_interface_type_identity(field.type, module)
                );
            }
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
            auto& exported_choice = package_interface.exported_type_declarations.back();
            for (auto const& variant : choice.variants) {
                auto identities = std::vector<GenericSpecializationTypeIdentity> {};
                for (auto const& payload : variant.payloads) {
                    identities.push_back(package_interface_type_identity(payload.type, module));
                }
                exported_choice.choice_variant_payload_type_identities.push_back(
                    std::move(identities)
                );
            }
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

    auto constants_by_name = std::unordered_map<std::string, syntax::ConstantSyntax const*> {};
    for (auto const& constant : module.constants) {
        constants_by_name.emplace(constant.name, &constant);
    }
    for (auto const& constant : module.constants) {
        if (constant.visibility != syntax::Visibility::public_visibility) {
            continue;
        }
        auto active_constants = std::unordered_set<std::string> {constant.name};
        auto initializer = expand_constant_expression(
            constant.initializer,
            constants_by_name,
            active_constants
        );
        package_interface.exported_constants.push_back(PackageConstantDeclaration {
            .name = constant.name,
            .type = constant.type,
            .type_identity = package_interface_type_identity(constant.type, module),
            .initializer_text = render_constant_expression(initializer),
        });
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
    auto constants = package_interface.exported_constants;
    std::ranges::sort(constants, {}, &PackageConstantDeclaration::name);

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
            auto const identity = declaration.aliased_type_identity.value_or(
                default_type_identity(*declaration.aliased_type)
            );
            output << "alias " << declaration.name << ' '
                   << render_type(*declaration.aliased_type) << ' '
                   << render_type_identity(identity) << '\n';
        }
        for (auto index = std::size_t {0}; index < declaration.public_record_fields.size(); ++index) {
            auto const& field = declaration.public_record_fields[index];
            auto const identity = index < declaration.public_record_field_type_identities.size()
                ? declaration.public_record_field_type_identities[index]
                : default_type_identity(field.type);
            output << "field " << declaration.name << ' ' << field.name << ' '
                   << render_type(field.type) << ' ' << render_type_identity(identity) << '\n';
        }
        for (auto variant_index = std::size_t {0};
             variant_index < declaration.choice_variants.size();
             ++variant_index) {
            auto const& variant = declaration.choice_variants[variant_index];
            output << "variant " << declaration.name << ' ' << variant.name << ' ' << variant.payloads.size();
            for (auto payload_index = std::size_t {0}; payload_index < variant.payloads.size(); ++payload_index) {
                auto const& payload = variant.payloads[payload_index];
                auto const has_identity =
                    variant_index < declaration.choice_variant_payload_type_identities.size() &&
                    payload_index < declaration.choice_variant_payload_type_identities[variant_index].size();
                auto const identity = has_identity
                    ? declaration.choice_variant_payload_type_identities[variant_index][payload_index]
                    : default_type_identity(payload.type);
                output << ' ' << payload.name << ' ' << render_type(payload.type)
                       << ' ' << render_type_identity(identity);
            }
            output << '\n';
        }
    }
    for (auto const& constant : constants) {
        output << "constant " << constant.name << ' ' << render_type(constant.type) << ' '
               << render_type_identity(constant.type_identity) << ' ' << constant.initializer_text << '\n';
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
    auto exported_constant_names = std::unordered_set<std::string> {};
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
            auto identity_text = std::string {};
            if (!(declaration_line >> owner_name >> type_text >> identity_text) || declaration_line >> extra) {
                return parse_error("invalid package interface alias declaration at line " + std::to_string(line_number));
            }
            auto* owner = find_type_declaration(result, owner_name, PackageTypeDeclarationKind::type_alias);
            auto target = parse_type(type_text);
            auto target_identity = parse_type_identity(identity_text);
            if (owner == nullptr || owner->aliased_type.has_value() || !target.has_value() ||
                !target_identity.has_value()) {
                return parse_error("invalid package interface alias declaration at line " + std::to_string(line_number));
            }
            owner->aliased_type = std::move(*target);
            owner->aliased_type_identity = std::move(*target_identity);
            continue;
        }

        if (record_kind == "field") {
            auto owner_name = std::string {};
            auto field_name = std::string {};
            auto type_text = std::string {};
            auto identity_text = std::string {};
            if (!(declaration_line >> owner_name >> field_name >> type_text >> identity_text) ||
                declaration_line >> extra) {
                return parse_error("invalid package interface field declaration at line " + std::to_string(line_number));
            }
            auto* owner = find_type_declaration(result, owner_name, PackageTypeDeclarationKind::record);
            auto field_type = parse_type(type_text);
            auto field_identity = parse_type_identity(identity_text);
            if (owner == nullptr || !field_type.has_value() || !field_identity.has_value() || std::ranges::any_of(
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
            owner->public_record_field_type_identities.push_back(std::move(*field_identity));
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
            auto payload_identities = std::vector<GenericSpecializationTypeIdentity> {};
            for (auto index = std::size_t {0}; index < *payload_count; ++index) {
                auto payload_name = std::string {};
                auto payload_type_text = std::string {};
                auto payload_identity_text = std::string {};
                if (!(declaration_line >> payload_name >> payload_type_text >> payload_identity_text)) {
                    return parse_error("invalid package interface variant declaration at line " + std::to_string(line_number));
                }
                auto payload_type = parse_type(payload_type_text);
                auto payload_identity = parse_type_identity(payload_identity_text);
                if (!payload_type.has_value() || !payload_identity.has_value()) {
                    return parse_error("invalid package interface variant declaration at line " + std::to_string(line_number));
                }
                variant.payloads.push_back(syntax::NamedTypeSyntax {
                    .name = std::move(payload_name),
                    .type = std::move(*payload_type),
                });
                payload_identities.push_back(std::move(*payload_identity));
            }
            if (declaration_line >> extra) {
                return parse_error("invalid package interface variant declaration at line " + std::to_string(line_number));
            }
            owner->choice_variants.push_back(std::move(variant));
            owner->choice_variant_payload_type_identities.push_back(std::move(payload_identities));
            continue;
        }

        if (record_kind == "constant") {
            auto constant = PackageConstantDeclaration {};
            auto type_text = std::string {};
            auto identity_text = std::string {};
            if (!(declaration_line >> constant.name >> type_text >> identity_text) ||
                !exported_constant_names.insert(constant.name).second) {
                return parse_error(
                    "invalid package interface constant declaration at line " +
                    std::to_string(line_number)
                );
            }
            std::getline(declaration_line, constant.initializer_text);
            constant.initializer_text = std::string(trimmed_text(constant.initializer_text));
            auto type = parse_type(type_text);
            auto identity = parse_type_identity(identity_text);
            if (!type.has_value() || !identity.has_value() || constant.initializer_text.empty() ||
                !parse_constant_declaration(
                    constant.name,
                    *type,
                    constant.initializer_text
                ).has_value()) {
                return parse_error(
                    "invalid package interface constant declaration at line " +
                    std::to_string(line_number)
                );
            }
            constant.type = std::move(*type);
            constant.type_identity = std::move(*identity);
            result.exported_constants.push_back(std::move(constant));
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

auto serialize_generic_function_specialization_requests(
    std::vector<GenericFunctionSpecializationRequest> const& requests
) -> std::string {
    auto sorted_requests = requests;
    std::ranges::sort(
        sorted_requests,
        [](GenericFunctionSpecializationRequest const& left, GenericFunctionSpecializationRequest const& right) {
            return specialization_request_sort_key(left) < specialization_request_sort_key(right);
        }
    );

    auto output = std::ostringstream {};
    output << "orison-specialization-requests " << generic_specialization_request_format_version << '\n';
    auto previous_key = std::optional<std::string> {};
    for (auto const& request : sorted_requests) {
        auto key = specialization_request_sort_key(request);
        if (previous_key == key) {
            continue;
        }
        previous_key = std::move(key);
        output << "request " << request.package_name << ' ' << request.declaration_name << ' '
               << request.type_arguments.size();
        for (auto index = std::size_t {0}; index < request.type_arguments.size(); ++index) {
            auto identity = index < request.type_identities.size()
                ? request.type_identities[index]
                : default_type_identity(request.type_arguments[index]);
            output << ' ' << render_type(canonical_type_from_identity(identity))
                   << ' ' << render_type_identity(identity);
        }
        output << '\n';
    }
    return output.str();
}

auto parse_generic_function_specialization_requests(std::string_view text)
    -> GenericFunctionSpecializationRequestParseResult {
    auto input = std::istringstream {std::string(text)};
    auto line = std::string {};
    if (!std::getline(input, line)) {
        return specialization_request_parse_error("specialization request artifact is empty");
    }
    remove_carriage_return(line);

    auto header = std::istringstream {line};
    auto marker = std::string {};
    auto version = std::size_t {};
    auto extra = std::string {};
    if (!(header >> marker >> version) || marker != "orison-specialization-requests" || header >> extra) {
        return specialization_request_parse_error("invalid specialization request header");
    }
    if (version != generic_specialization_request_format_version) {
        return specialization_request_parse_error(
            "unsupported specialization request version '" + std::to_string(version) + "'"
        );
    }

    auto result = GenericFunctionSpecializationRequestParseResult {};
    auto request_keys = std::unordered_set<std::string> {};
    auto line_number = std::size_t {1};
    while (std::getline(input, line)) {
        ++line_number;
        remove_carriage_return(line);
        auto request_line = std::istringstream {line};
        auto record_kind = std::string {};
        auto type_argument_count_text = std::string {};
        auto request = GenericFunctionSpecializationRequest {};
        if (!(request_line >> record_kind >> request.package_name >> request.declaration_name >>
              type_argument_count_text) ||
            record_kind != "request") {
            return specialization_request_parse_error(
                "invalid specialization request at line " + std::to_string(line_number)
            );
        }
        auto type_argument_count = parse_count(type_argument_count_text);
        if (!type_argument_count.has_value() || *type_argument_count == 0) {
            return specialization_request_parse_error(
                "invalid specialization request type argument count at line " +
                std::to_string(line_number)
            );
        }
        for (auto index = std::size_t {0}; index < *type_argument_count; ++index) {
            auto type_text = std::string {};
            auto identity_text = std::string {};
            if (!(request_line >> type_text >> identity_text)) {
                return specialization_request_parse_error(
                    "invalid specialization request at line " + std::to_string(line_number)
                );
            }
            auto type = parse_type(type_text);
            if (!type.has_value()) {
                return specialization_request_parse_error(
                    "invalid specialization request type at line " + std::to_string(line_number)
                );
            }
            request.type_arguments.push_back(std::move(*type));
            auto identity = parse_type_identity(identity_text);
            if (!identity.has_value()) {
                return specialization_request_parse_error(
                    "invalid specialization request type identity at line " +
                    std::to_string(line_number)
                );
            }
            request.type_identities.push_back(std::move(*identity));
        }
        if (request_line >> extra) {
            return specialization_request_parse_error(
                "invalid specialization request at line " + std::to_string(line_number)
            );
        }
        auto key = specialization_request_sort_key(request);
        if (!request_keys.insert(std::move(key)).second) {
            return specialization_request_parse_error(
                "duplicate specialization request at line " + std::to_string(line_number)
            );
        }
        result.requests.push_back(std::move(request));
    }
    return result;
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

            auto lowering_alias_target = declaration->kind == PackageTypeDeclarationKind::type_alias
                ? resolve_package_alias_target(
                      GenericSpecializationTypeIdentity {
                          .package_name = package_interface.package_name,
                          .declaration_name = declaration->name,
                      },
                      package_interface,
                      package_interfaces
                  )
                : std::nullopt;
            if (lowering_alias_target.has_value()) {
                lowering_alias_target = localize_imported_type(
                    std::move(*lowering_alias_target),
                    module,
                    package_interface.package_name
                );
            }

            imported_declarations.push_back(ImportedTypeDeclarationMetadata {
                .package_name = package_interface.package_name,
                .declaration_name = declaration->name,
                .local_name = import.alias.empty() ? import.name : import.alias,
                .generic_arity = declaration->generic_arity,
                .lowering_alias_target = std::move(lowering_alias_target),
            });
            break;
        }
    }

    return imported_declarations;
}

auto resolve_imported_aggregate_declarations(
    syntax::ModuleSyntax const& module,
    std::vector<PackageInterface> const& package_interfaces
) -> ImportedAggregateDeclarations {
    auto imported_declarations = ImportedAggregateDeclarations {};

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

            auto const local_name = import.alias.empty() ? import.name : import.alias;
            auto resolved = declaration->kind == PackageTypeDeclarationKind::type_alias
                ? resolve_package_aggregate(
                      GenericSpecializationTypeIdentity {
                          .package_name = package_interface.package_name,
                          .declaration_name = declaration->name,
                      },
                      package_interface,
                      package_interfaces
                  )
                : ResolvedPackageAggregate {
                      .declaration = &*declaration,
                  };
            if (resolved.declaration == nullptr) {
                break;
            }
            if (resolved.declaration->kind == PackageTypeDeclarationKind::record) {
                imported_declarations.records.push_back(syntax::RecordSyntax {
                    .visibility = syntax::Visibility::public_visibility,
                    .name = local_name,
                    .generic_parameters = resolved.declaration->generic_parameters,
                    .fields = resolved.declaration->public_record_fields,
                });
            } else if (resolved.declaration->kind == PackageTypeDeclarationKind::choice) {
                imported_declarations.choices.push_back(syntax::ChoiceSyntax {
                    .visibility = syntax::Visibility::public_visibility,
                    .name = local_name,
                    .generic_parameters = resolved.declaration->generic_parameters,
                    .variants = resolved.declaration->choice_variants,
                });
            }
            break;
        }
    }

    return imported_declarations;
}

auto resolve_imported_function_declarations(
    syntax::ModuleSyntax const& module,
    std::vector<PackageInterface> const& package_interfaces
) -> std::vector<ImportedFunctionDeclaration> {
    auto imported_declarations = std::vector<ImportedFunctionDeclaration> {};

    for (auto const& import : module.imports) {
        for (auto const& package_interface : package_interfaces) {
            if (package_interface.package_name != import.from_package) {
                continue;
            }

            auto declaration = std::ranges::find_if(
                package_interface.exported_functions,
                [&](PackageFunctionDeclaration const& candidate) {
                    return candidate.name == import.name;
                }
            );
            if (declaration == package_interface.exported_functions.end()) {
                continue;
            }

            auto localized = *declaration;
            for (auto& parameter : localized.parameters) {
                parameter.type = localize_imported_type(
                    std::move(parameter.type),
                    module,
                    package_interface.package_name
                );
            }
            localized.return_type = localize_imported_type(
                std::move(localized.return_type),
                module,
                package_interface.package_name
            );
            for (auto& constraint : localized.where_constraints) {
                for (auto& requirement : constraint.requirements) {
                    requirement = localize_imported_type(
                        std::move(requirement),
                        module,
                        package_interface.package_name
                    );
                }
            }
            imported_declarations.push_back(ImportedFunctionDeclaration {
                .package_name = package_interface.package_name,
                .declaration_name = declaration->name,
                .local_name = import.alias.empty() ? import.name : import.alias,
                .declaration = std::move(localized),
            });
            break;
        }
    }

    return imported_declarations;
}

auto resolve_imported_constant_declarations(
    syntax::ModuleSyntax const& module,
    std::vector<PackageInterface> const& package_interfaces
) -> std::vector<ImportedConstantDeclaration> {
    auto imported_declarations = std::vector<ImportedConstantDeclaration> {};
    for (auto const& import : module.imports) {
        auto package_interface = std::ranges::find_if(
            package_interfaces,
            [&](PackageInterface const& candidate) {
                return candidate.package_name == import.from_package;
            }
        );
        if (package_interface == package_interfaces.end()) {
            continue;
        }
        auto declaration = std::ranges::find_if(
            package_interface->exported_constants,
            [&](PackageConstantDeclaration const& candidate) {
                return candidate.name == import.name;
            }
        );
        if (declaration == package_interface->exported_constants.end()) {
            continue;
        }

        auto localized_type = localize_imported_type(
            declaration->type,
            module,
            package_interface->package_name
        );
        auto parsed = parse_constant_declaration(
            declaration->name,
            localized_type,
            declaration->initializer_text
        );
        if (!parsed.has_value()) {
            continue;
        }
        localize_imported_constant_expression(
            parsed->initializer,
            module,
            package_interface->package_name
        );
        imported_declarations.push_back(ImportedConstantDeclaration {
            .package_name = package_interface->package_name,
            .declaration_name = declaration->name,
            .local_name = import.alias.empty() ? import.name : import.alias,
            .type = std::move(localized_type),
            .initializer_text = render_constant_expression(parsed->initializer),
        });
    }
    return imported_declarations;
}

auto materialize_imported_constant(
    ImportedConstantDeclaration const& declaration
) -> std::optional<syntax::ConstantSyntax> {
    auto constant = parse_constant_declaration(
        declaration.local_name,
        declaration.type,
        declaration.initializer_text
    );
    if (constant.has_value()) {
        constant->line = 0;
    }
    return constant;
}

}  // namespace orison::semantics
