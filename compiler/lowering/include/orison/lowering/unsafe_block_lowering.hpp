#pragma once

#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/loop_lowering_support.hpp"

#include <functional>

namespace orison::lowering {

using UnsafeBlockBodyLowerer = std::function<StatementFlow()>;

auto lower_unsafe_block(
    FunctionLoweringSession& session,
    UnsafeBlockBodyLowerer lower_body
) -> StatementFlow;

}  // namespace orison::lowering
