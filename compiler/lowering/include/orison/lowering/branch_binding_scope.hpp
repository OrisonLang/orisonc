#pragma once

#include "orison/lowering/function_lowering_state.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace orison::lowering {

class BranchBindingScope {
public:
    explicit BranchBindingScope(FunctionLoweringState& state);
    ~BranchBindingScope() noexcept;

    BranchBindingScope(BranchBindingScope const&) = delete;
    auto operator=(BranchBindingScope const&) -> BranchBindingScope& = delete;
    BranchBindingScope(BranchBindingScope&&) = delete;
    auto operator=(BranchBindingScope&&) -> BranchBindingScope& = delete;

    void reset();

private:
    FunctionLoweringState& state_;
    std::unordered_map<std::string, LoweredExpression> saved_immutable_bindings_;
    std::unordered_map<std::string, MutableBinding> saved_mutable_bindings_;
    std::unordered_map<std::string, AddressableBinding> saved_addressable_bindings_;
    std::unordered_map<std::string, std::string> saved_source_type_names_;
    std::unordered_set<std::string> saved_consumed_owned_dynamic_array_locals_;
};

}  // namespace orison::lowering
