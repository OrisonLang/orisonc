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

}  // namespace orison::lowering
