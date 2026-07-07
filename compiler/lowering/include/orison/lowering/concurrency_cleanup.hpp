#pragma once

#include "orison/lowering/function_lowering_session.hpp"

#include <sstream>

namespace orison::lowering {

auto emit_abandoned_concurrency_handle_cleanup(
    FunctionLoweringSession& session,
    std::ostringstream& output
) -> void;

}  // namespace orison::lowering
