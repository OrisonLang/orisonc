#include "orison/lowering/deferred_cleanup.hpp"

namespace orison::lowering {

DeferredCleanupScope::DeferredCleanupScope(FunctionLoweringState& state)
    : state_(state),
      cleanup_depth_(state.defer_cleanup_scopes.size()) {
    state_.defer_cleanup_scopes.emplace_back();
}

DeferredCleanupScope::~DeferredCleanupScope() noexcept {
    state_.defer_cleanup_scopes.pop_back();
}

auto DeferredCleanupScope::cleanup_depth() const -> std::size_t {
    return cleanup_depth_;
}

auto emit_deferred_cleanup_to_depth(
    std::size_t target_depth,
    LoweringEmissionContext const& context,
    FunctionLoweringSession& session,
    diagnostics::DiagnosticBag& diagnostics,
    std::ostringstream& output,
    DeferredCleanupBlockLowerer lower_cleanup_block
) -> bool {
    if (target_depth > session.state.defer_cleanup_scopes.size()) {
        diagnostics.error(1, "lowering defer cleanup depth is inconsistent with the active scope stack");
        return false;
    }

    auto scope_index = session.state.defer_cleanup_scopes.size();
    auto scope_output = std::ostringstream {};
    while (scope_index > target_depth) {
        auto const scope = session.state.defer_cleanup_scopes[scope_index - 1];
        auto cleanup_output = std::ostringstream {};
        auto const blocks = scope.blocks;
        for (auto block_index = blocks.size(); block_index-- > 0;) {
            auto const& block = blocks[block_index];
            auto block_output = std::ostringstream {};
            auto flow = lower_cleanup_block(
                block.statements,
                context,
                session,
                diagnostics,
                block_output
            );
            if (flow == StatementFlow::failed) {
                return false;
            }
            if (flow == StatementFlow::terminated) {
                diagnostics.error(
                    block.line,
                    "lowering defer cleanup blocks currently requires them to fall through"
                );
                return false;
            }
            cleanup_output << block_output.str();
        }
        scope_output << cleanup_output.str();
        --scope_index;
    }

    output << scope_output.str();
    return true;
}

}  // namespace orison::lowering
