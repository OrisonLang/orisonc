#pragma once

#include "orison/semantics/drop_model.hpp"

#include <string_view>
#include <vector>

namespace orison::lowering {

struct LlvmIrEmissionOptions {
    // Test seam only. Do not expose this as user/compiler-driver surface.
    std::vector<std::string_view> test_only_declared_drop_source_type_allowlist;
    std::vector<semantics::DropLoweringAuthorization> semantic_drop_lowering_authorizations;
};

}  // namespace orison::lowering
