#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace orison::lowering {

struct LoweringContext;

struct RuntimeIndexedPartialOwner {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::string cleanup_strategy;
    bool constructor_move_enabled = false;

    auto operator==(RuntimeIndexedPartialOwner const&) const -> bool = default;
};

struct RuntimeIndexedCleanupSkipPlan {
    std::string owner_name;
    std::string index_expression_text;
    std::string element_source_type_name;
    std::string moved_source_type_name;
    std::string cleanup_operation;
    bool production_cleanup_enabled = false;

    auto operator==(RuntimeIndexedCleanupSkipPlan const&) const -> bool = default;
};

struct OwnershipTransferState {
    std::unordered_set<std::string> consumed_owned_bindings;
    std::vector<RuntimeIndexedPartialOwner> runtime_indexed_partial_owners;
    std::vector<RuntimeIndexedCleanupSkipPlan> runtime_indexed_cleanup_skip_plans;
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

auto record_runtime_indexed_partial_owner(
    OwnershipTransferState& state,
    RuntimeIndexedPartialOwner owner
) -> void;

auto runtime_indexed_partial_owner_report(
    RuntimeIndexedPartialOwner const& owner
) -> std::string;

auto runtime_indexed_cleanup_skip_plan(
    RuntimeIndexedPartialOwner const& owner
) -> RuntimeIndexedCleanupSkipPlan;

auto runtime_indexed_cleanup_skip_plan_report(
    RuntimeIndexedCleanupSkipPlan const& plan
) -> std::string;

auto is_owned_binding_consumed(
    OwnershipTransferState const& state,
    std::string_view binding_name
) -> bool;

auto consumed_owned_binding_or_descendant_name(
    OwnershipTransferState const& state,
    std::string_view binding_name
) -> std::optional<std::string>;

auto consumed_owned_descendant_names(
    std::vector<OwnershipTransferState> const& states,
    std::string_view owner_name
) -> std::vector<std::string>;

auto normalize_consumed_owned_descendants(
    std::vector<OwnershipTransferState>& states,
    std::vector<std::string> const& consumed_descendant_names
) -> void;

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
