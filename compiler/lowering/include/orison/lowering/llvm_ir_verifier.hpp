#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"

#include <string_view>

namespace orison::lowering {

class LlvmIrVerifier {
public:
    auto verify(std::string_view ir_text) const -> diagnostics::DiagnosticBag;
};

}  // namespace orison::lowering
