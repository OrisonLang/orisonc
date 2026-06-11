#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"

#include <string_view>

namespace orison::link {

struct HostRunResult {
    diagnostics::DiagnosticBag diagnostics;
    int exit_code = 1;

    auto has_errors() const -> bool;
};

class HostRunner {
public:
    auto run(std::string_view object_bytes) const -> HostRunResult;
};

}  // namespace orison::link
