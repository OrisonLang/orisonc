#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"

#include <string>
#include <string_view>

namespace orison::lowering {

struct LlvmObjectEmissionResult {
    diagnostics::DiagnosticBag diagnostics;
    std::string object_bytes;

    auto has_errors() const -> bool;
};

class LlvmObjectEmitter {
public:
    auto emit(std::string_view ir_text) const -> LlvmObjectEmissionResult;
};

}  // namespace orison::lowering
