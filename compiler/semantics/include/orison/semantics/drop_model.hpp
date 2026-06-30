#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace orison::semantics {

struct DropImplementation {
    std::string source_type_name;
    std::string abi_symbol_name;
    std::size_t declaration_line = 0;
    bool proven = false;
};

struct PlannedDropSite {
    std::string source_type_name;
    std::string abi_symbol_name;
    std::string owner_name;
    std::size_t site_line = 0;
};

struct DropImplementationResolution {
    PlannedDropSite site;
    bool resolved = false;
};

auto drop_abi_symbol_name(std::string_view source_type_name) -> std::string;

auto format_drop_implementation(DropImplementation const& implementation) -> std::string;

auto format_planned_drop_site(PlannedDropSite const& site) -> std::string;

auto resolve_drop_implementation(
    PlannedDropSite site,
    std::vector<DropImplementation> const& implementations
) -> DropImplementationResolution;

auto format_drop_implementation_resolution(
    DropImplementationResolution const& resolution
) -> std::string;

}  // namespace orison::semantics
