#pragma once

#include "orison/lowering/function_lowering_state.hpp"
#include "orison/lowering/lowering_failures.hpp"

namespace orison::lowering {

struct FunctionLoweringSession {
    FunctionLoweringState& state;
    LoweringFailures& failures;
};

}  // namespace orison::lowering
