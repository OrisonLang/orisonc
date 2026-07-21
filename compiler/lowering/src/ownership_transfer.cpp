#include "orison/lowering/ownership_transfer.hpp"

#include <cstddef>
#include <utility>

namespace orison::lowering {

auto mark_owned_binding_consumed(
    OwnershipTransferState& state,
    std::string binding_name
) -> void {
    state.consumed_owned_bindings.insert(std::move(binding_name));
}

auto is_owned_binding_consumed(
    OwnershipTransferState const& state,
    std::string_view binding_name
) -> bool {
    return state.consumed_owned_bindings.contains(std::string(binding_name));
}

auto merge_ownership_transfer_states(
    std::vector<OwnershipTransferState> const& branch_states
) -> std::optional<OwnershipTransferState> {
    if (branch_states.empty()) {
        return OwnershipTransferState {};
    }

    auto merged = branch_states.front();
    for (auto index = std::size_t {1}; index < branch_states.size(); ++index) {
        if (branch_states[index].consumed_owned_bindings != merged.consumed_owned_bindings) {
            return std::nullopt;
        }
    }
    return merged;
}

}  // namespace orison::lowering
