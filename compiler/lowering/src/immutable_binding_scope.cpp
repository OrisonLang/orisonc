#include "orison/lowering/immutable_binding_scope.hpp"

namespace orison::lowering {

ImmutableBindingScope::ImmutableBindingScope(FunctionLoweringState& state)
    : state_(state), saved_bindings_(state.immutable_bindings) {}

ImmutableBindingScope::~ImmutableBindingScope() noexcept {
    state_.immutable_bindings.swap(saved_bindings_);
}

void ImmutableBindingScope::reset() {
    state_.immutable_bindings = saved_bindings_;
}

}  // namespace orison::lowering
