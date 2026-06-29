#include "orison/lowering/drop_metadata.hpp"

#include <sstream>
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

auto add_planned_drop_declaration(
    std::vector<PlannedDropDeclaration>& declarations,
    PlannedDropDeclaration declaration
) -> bool {
    for (auto const& existing_declaration : declarations) {
        if (existing_declaration.symbol_name == declaration.symbol_name) {
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

}  // namespace orison::lowering
