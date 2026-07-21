#pragma once

#include "orison/lowering/function_lowering_state.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
    void commit_consumed_owned_dynamic_array_bindings(std::unordered_set<std::string> bindings);
    [[nodiscard]] auto saved_consumed_owned_dynamic_array_bindings() const
        -> std::unordered_set<std::string> const&;

private:
    FunctionLoweringState& state_;
    std::unordered_map<std::string, LoweredExpression> saved_immutable_bindings_;
    std::unordered_map<std::string, MutableBinding> saved_mutable_bindings_;
    std::unordered_map<std::string, AddressableBinding> saved_addressable_bindings_;
    std::unordered_map<std::string, std::string> saved_source_type_names_;
    std::unordered_set<std::string> saved_consumed_owned_dynamic_array_bindings_;
};

auto merge_consumed_owned_dynamic_array_bindings(
    std::vector<std::unordered_set<std::string>> const& branch_bindings
) -> std::optional<std::unordered_set<std::string>>;

}  // namespace orison::lowering
