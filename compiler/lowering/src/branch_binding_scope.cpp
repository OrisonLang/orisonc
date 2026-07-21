#include "orison/lowering/branch_binding_scope.hpp"

#include <cstddef>
#include <utility>

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

void BranchBindingScope::commit_consumed_owned_dynamic_array_bindings(
    std::unordered_set<std::string> bindings
) {
    saved_consumed_owned_dynamic_array_bindings_ = std::move(bindings);
    state_.consumed_owned_dynamic_array_bindings = saved_consumed_owned_dynamic_array_bindings_;
}

auto BranchBindingScope::saved_consumed_owned_dynamic_array_bindings() const
    -> std::unordered_set<std::string> const& {
    return saved_consumed_owned_dynamic_array_bindings_;
}

auto merge_consumed_owned_dynamic_array_bindings(
    std::vector<std::unordered_set<std::string>> const& branch_bindings
) -> std::optional<std::unordered_set<std::string>> {
    if (branch_bindings.empty()) {
        return std::unordered_set<std::string> {};
    }

    auto merged = branch_bindings.front();
    for (auto index = std::size_t {1}; index < branch_bindings.size(); ++index) {
        if (branch_bindings[index] != merged) {
            return std::nullopt;
        }
    }
    return merged;
}

}  // namespace orison::lowering
