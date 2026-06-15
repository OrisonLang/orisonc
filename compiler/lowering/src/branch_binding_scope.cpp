#include "orison/lowering/branch_binding_scope.hpp"

namespace orison::lowering {

BranchBindingScope::BranchBindingScope(FunctionLoweringState& state)
    : state_(state),
      saved_immutable_bindings_(state.immutable_bindings),
      saved_mutable_bindings_(state.mutable_bindings),
      saved_source_type_names_(state.source_type_names) {}

BranchBindingScope::~BranchBindingScope() noexcept {
    state_.immutable_bindings.swap(saved_immutable_bindings_);
    state_.mutable_bindings.swap(saved_mutable_bindings_);
    state_.source_type_names.swap(saved_source_type_names_);
}

void BranchBindingScope::reset() {
    state_.immutable_bindings = saved_immutable_bindings_;
    state_.mutable_bindings = saved_mutable_bindings_;
    state_.source_type_names = saved_source_type_names_;
}

}  // namespace orison::lowering
