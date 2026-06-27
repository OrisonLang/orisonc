#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace orison::lowering {

struct PlannedDropDeclaration {
    std::string symbol_name;
    std::string source_type_name;
    std::size_t discovery_line = 0;
    bool emit_declaration = false;
};

auto format_planned_drop_declaration(PlannedDropDeclaration const& declaration) -> std::string;

auto add_planned_drop_declaration(
    std::vector<PlannedDropDeclaration>& declarations,
    PlannedDropDeclaration declaration
) -> bool;

}  // namespace orison::lowering
