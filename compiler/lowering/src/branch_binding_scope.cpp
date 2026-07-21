#include "orison/lowering/branch_binding_scope.hpp"

namespace orison::lowering {

BranchBindingScope::BranchBindingScope(FunctionLoweringState& state)
    : state_(state),
      saved_immutable_bindings_(state.immutable_bindings),
      saved_mutable_bindings_(state.mutable_bindings),
      saved_addressable_bindings_(state.addressable_bindings),
      saved_source_type_names_(state.source_type_names),
      saved_consumed_owned_dynamic_array_bindings_(state.consumed_owned_dynamic_array_bindings) {}

BranchBindingScope::~BranchBindingScope() noexcept {
    state_.immutable_bindings.swap(saved_immutable_bindings_);
    state_.mutable_bindings.swap(saved_mutable_bindings_);
    state_.addressable_bindings.swap(saved_addressable_bindings_);
    state_.source_type_names.swap(saved_source_type_names_);
    state_.consumed_owned_dynamic_array_bindings.swap(saved_consumed_owned_dynamic_array_bindings_);
}

void BranchBindingScope::reset() {
    state_.immutable_bindings = saved_immutable_bindings_;
    state_.mutable_bindings = saved_mutable_bindings_;
    state_.addressable_bindings = saved_addressable_bindings_;
    state_.source_type_names = saved_source_type_names_;
    state_.consumed_owned_dynamic_array_bindings = saved_consumed_owned_dynamic_array_bindings_;
}

}  // namespace orison::lowering
