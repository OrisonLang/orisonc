#pragma once

#include "orison/semantics/drop_model.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct PlannedDropDeclaration {
    std::string symbol_name;
    std::string source_type_name;
    std::size_t discovery_line = 0;
    bool emit_declaration = false;
};

struct PlannedDropAction {
    std::string capture_name;
    std::string source_type_name;
    std::string symbol_name;
    std::size_t field_index = 0;
    std::size_t discovery_line = 0;
};

auto format_planned_drop_declaration(PlannedDropDeclaration const& declaration) -> std::string;

auto format_planned_drop_action(PlannedDropAction const& action) -> std::string;

auto format_planned_drop_report(
    std::vector<PlannedDropDeclaration> const& declarations
) -> std::vector<std::string>;

auto format_emitted_drop_declaration_report(
    std::vector<PlannedDropDeclaration> const& declarations
) -> std::vector<std::string>;

auto format_planned_drop_action_report(
    std::vector<PlannedDropAction> const& actions
) -> std::vector<std::string>;

auto add_planned_drop_declaration(
    std::vector<PlannedDropDeclaration>& declarations,
    PlannedDropDeclaration declaration
) -> bool;

auto planned_drop_declaration_for_action(PlannedDropAction const& action) -> PlannedDropDeclaration;

auto planned_drop_declaration_for_authorization(
    semantics::DropLoweringAuthorization const& authorization
) -> PlannedDropDeclaration;

auto declared_drop_declarations_for_authorized_semantic_drops(
    std::vector<semantics::DropLoweringAuthorization> const& authorizations
) -> std::vector<PlannedDropDeclaration>;

auto declared_drop_declarations_for_allowed_source_types(
    std::vector<PlannedDropAction> const& actions,
    std::vector<std::string_view> const& allowed_source_type_names
) -> std::vector<PlannedDropDeclaration>;

}  // namespace orison::lowering
