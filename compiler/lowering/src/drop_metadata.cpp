#include "orison/lowering/drop_metadata.hpp"

#include <algorithm>
#include <sstream>
#include <string_view>
#include <utility>

namespace orison::lowering {

auto format_planned_drop_declaration(PlannedDropDeclaration const& declaration) -> std::string {
    auto output = std::ostringstream {};
    output << "planned drop " << declaration.symbol_name;
    if (!declaration.source_type_name.empty()) {
        output << " for " << declaration.source_type_name;
    }
    if (declaration.discovery_line > 0) {
        output << " discovered at line " << declaration.discovery_line;
    }
    if (!declaration.emit_declaration) {
        output << " (metadata only)";
    }
    return output.str();
}

auto format_planned_drop_action(PlannedDropAction const& action) -> std::string {
    auto output = std::ostringstream {};
    output << "planned drop action " << action.symbol_name;
    if (!action.capture_name.empty()) {
        output << " for capture " << action.capture_name;
    }
    if (!action.source_type_name.empty()) {
        output << ": " << action.source_type_name;
    }
    output << " field " << action.field_index;
    if (action.discovery_line > 0) {
        output << " discovered at line " << action.discovery_line;
    }
    output << " (metadata only)";
    return output.str();
}

auto format_planned_drop_report(
    std::vector<PlannedDropDeclaration> const& declarations
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(declarations.size());
    for (auto const& declaration : declarations) {
        report.push_back(format_planned_drop_declaration(declaration));
    }
    return report;
}

auto format_planned_drop_action_report(
    std::vector<PlannedDropAction> const& actions
) -> std::vector<std::string> {
    auto report = std::vector<std::string> {};
    report.reserve(actions.size());
    for (auto const& action : actions) {
        report.push_back(format_planned_drop_action(action));
    }
    return report;
}

auto add_planned_drop_declaration(
    std::vector<PlannedDropDeclaration>& declarations,
    PlannedDropDeclaration declaration
) -> bool {
    for (auto& existing_declaration : declarations) {
        if (existing_declaration.symbol_name == declaration.symbol_name) {
            existing_declaration.emit_declaration =
                existing_declaration.emit_declaration || declaration.emit_declaration;
            return false;
        }
    }
    declarations.push_back(std::move(declaration));
    return true;
}

auto planned_drop_declaration_for_action(PlannedDropAction const& action) -> PlannedDropDeclaration {
    return PlannedDropDeclaration {
        .symbol_name = action.symbol_name,
        .source_type_name = action.source_type_name,
        .discovery_line = action.discovery_line,
    };
}

auto planned_drop_declaration_for_authorization(
    semantics::DropLoweringAuthorization const& authorization
) -> PlannedDropDeclaration {
    return PlannedDropDeclaration {
        .symbol_name = authorization.site.abi_symbol_name,
        .source_type_name = authorization.site.source_type_name,
        .discovery_line = authorization.site.site_line,
        .emit_declaration = authorization.authorized,
    };
}

auto declared_drop_declarations_for_authorized_semantic_drops(
    std::vector<semantics::DropLoweringAuthorization> const& authorizations
) -> std::vector<PlannedDropDeclaration> {
    auto declarations = std::vector<PlannedDropDeclaration> {};
    for (auto const& authorization : authorizations) {
        if (!authorization.authorized) {
            continue;
        }
        add_planned_drop_declaration(
            declarations,
            planned_drop_declaration_for_authorization(authorization)
        );
    }
    return declarations;
}

auto declared_drop_declarations_for_allowed_source_types(
    std::vector<PlannedDropAction> const& actions,
    std::vector<std::string_view> const& allowed_source_type_names
) -> std::vector<PlannedDropDeclaration> {
    auto declarations = std::vector<PlannedDropDeclaration> {};
    for (auto const& action : actions) {
        auto const allowed = std::find(
            allowed_source_type_names.begin(),
            allowed_source_type_names.end(),
            action.source_type_name
        ) != allowed_source_type_names.end();
        if (!allowed) {
            continue;
        }
        auto declaration = planned_drop_declaration_for_action(action);
        declaration.emit_declaration = true;
        add_planned_drop_declaration(declarations, std::move(declaration));
    }
    return declarations;
}

}  // namespace orison::lowering
