#pragma once

#include "orison/semantics/drop_model.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct OwnedCleanupDeclaration {
    std::string symbol_name;
    std::string source_type_name;
    std::size_t discovery_line = 0;
    bool emit_declaration = false;
};

struct OwnedCleanupAction {
    std::string capture_name;
    std::string source_type_name;
    std::string symbol_name;
    std::size_t field_index = 0;
    std::size_t discovery_line = 0;
};

auto format_owned_cleanup_declaration(OwnedCleanupDeclaration const& declaration) -> std::string;

auto format_owned_cleanup_action(OwnedCleanupAction const& action) -> std::string;

auto format_owned_cleanup_declaration_report(
    std::vector<OwnedCleanupDeclaration> const& declarations
) -> std::vector<std::string>;

auto format_emitted_owned_cleanup_declaration_report(
    std::vector<OwnedCleanupDeclaration> const& declarations
) -> std::vector<std::string>;

auto format_owned_cleanup_action_report(
    std::vector<OwnedCleanupAction> const& actions
) -> std::vector<std::string>;

auto add_owned_cleanup_declaration(
    std::vector<OwnedCleanupDeclaration>& declarations,
    OwnedCleanupDeclaration declaration
) -> bool;

auto owned_cleanup_declaration_for_action(OwnedCleanupAction const& action) -> OwnedCleanupDeclaration;

auto owned_cleanup_declaration_for_authorization(
    semantics::OwnedCleanupLoweringAuthorization const& authorization
) -> OwnedCleanupDeclaration;

auto declared_owned_cleanup_declarations_for_authorized_semantic_owned_cleanups(
    std::vector<semantics::OwnedCleanupLoweringAuthorization> const& authorizations
) -> std::vector<OwnedCleanupDeclaration>;

auto declared_owned_cleanup_declarations_for_allowed_source_types(
    std::vector<OwnedCleanupAction> const& actions,
    std::vector<std::string_view> const& allowed_source_type_names
) -> std::vector<OwnedCleanupDeclaration>;

}  // namespace orison::lowering
