#pragma once

#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/string_constants.hpp"

namespace orison::lowering {

struct LoweringEmissionContext {
    LoweringContext const& lowering;
    StringConstantTable const& string_constants;
};

}  // namespace orison::lowering
