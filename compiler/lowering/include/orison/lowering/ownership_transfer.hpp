#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace orison::lowering {

struct LoweringContext;

struct OwnershipTransferState {
    std::unordered_set<std::string> consumed_owned_bindings;
};

struct OwnedAggregateMemberTransfer {
    std::string binding_name;
    std::string owner_name;
    std::string member_name;
    std::string source_type_name;
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

auto owned_binding_member_name(
    std::string_view owner_name,
    std::string_view member_name
) -> std::string;

auto is_owned_transfer_source_type(
    std::string_view source_type_name,
    LoweringContext const& context
) -> bool;

auto owned_record_field_transfer(
    std::string_view owner_name,
    std::string_view owner_source_type_name,
    std::string_view field_name,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer>;

auto owned_record_member_path_transfer(
    std::string_view owner_name,
    std::string_view owner_source_type_name,
    std::span<std::string const> field_names,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer>;

auto owned_choice_payload_transfer(
    std::string_view owner_name,
    std::string_view choice_source_type_name,
    std::string_view variant_name,
    std::string_view payload_name,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer>;

}  // namespace orison::lowering
