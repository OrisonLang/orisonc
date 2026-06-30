#pragma once

#include <string_view>
#include <vector>

namespace orison::lowering {

struct LlvmIrEmissionOptions {
    std::vector<std::string_view> declared_drop_source_type_allowlist;
};

}  // namespace orison::lowering
