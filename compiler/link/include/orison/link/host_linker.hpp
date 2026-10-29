#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace orison::link {

struct HostLinkResult {
    diagnostics::DiagnosticBag diagnostics;

    auto has_errors() const -> bool;
};

class HostLinker {
public:
    auto link(
        std::string_view object_bytes,
        std::filesystem::path const& output_path,
        std::vector<std::string> const& libraries = {}
    ) const -> HostLinkResult;

    auto link(
        std::span<std::string_view const> object_images,
        std::filesystem::path const& output_path,
        std::vector<std::string> const& libraries = {}
    ) const -> HostLinkResult;
};

}  // namespace orison::link
