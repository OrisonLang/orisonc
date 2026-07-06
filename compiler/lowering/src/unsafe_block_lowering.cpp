#include "orison/lowering/unsafe_block_lowering.hpp"

#include "orison/lowering/branch_binding_scope.hpp"

namespace orison::lowering {

auto lower_unsafe_block(
    FunctionLoweringSession& session,
    UnsafeBlockBodyLowerer lower_body
) -> StatementFlow {
    [[maybe_unused]] auto binding_scope = BranchBindingScope(session.state);
    return lower_body();
}

}  // namespace orison::lowering
