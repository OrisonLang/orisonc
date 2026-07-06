#pragma once

#include "orison/lowering/function_lowering_session.hpp"
#include "orison/lowering/branch_binding_scope.hpp"
#include "orison/lowering/loop_lowering_support.hpp"

namespace orison::lowering {

template <typename LowerBody>
auto lower_unsafe_block(
    FunctionLoweringSession& session,
    LowerBody&& lower_body
) -> StatementFlow {
    [[maybe_unused]] auto binding_scope = BranchBindingScope(session.state);
    return lower_body();
}

}  // namespace orison::lowering
