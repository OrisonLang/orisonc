#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace orison::lowering {

struct OwnershipTransferState {
    std::unordered_set<std::string> consumed_owned_bindings;
};

auto mark_owned_binding_consumed(
    OwnershipTransferState& state,
    std::string binding_name
) -> void;

auto is_owned_binding_consumed(
    OwnershipTransferState const& state,
    std::string_view binding_name
) -> bool;

auto merge_ownership_transfer_states(
    std::vector<OwnershipTransferState> const& branch_states
) -> std::optional<OwnershipTransferState>;

}  // namespace orison::lowering
