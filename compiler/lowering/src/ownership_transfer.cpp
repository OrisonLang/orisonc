#include "orison/lowering/ownership_transfer.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "orison/lowering/lowering_context.hpp"
#include "orison/lowering/source_type_queries.hpp"

namespace orison::lowering {
namespace {

auto is_owned_transfer_source_type_impl(
    std::string_view source_type_name,
    LoweringContext const& context,
    std::unordered_set<std::string>& visiting
) -> bool {
    if (source_type_name.empty() || is_scalar_or_nonowning_source_type(source_type_name)) {
        return false;
    }

    auto source_type_key = std::string {source_type_name};
    if (!visiting.insert(source_type_key).second) {
        return true;
    }

    if (dynamic_array_element_source_type_name(source_type_name).has_value()) {
        visiting.erase(source_type_key);
        return true;
    }

    if (auto array_element_type = array_element_source_type_name(source_type_name)) {
        auto owns = is_owned_transfer_source_type_impl(*array_element_type, context, visiting);
        visiting.erase(source_type_key);
        return owns;
    }

    if (auto maybe_payload_type = maybe_payload_source_type_name(source_type_name)) {
        auto owns = is_owned_transfer_source_type_impl(*maybe_payload_type, context, visiting);
        visiting.erase(source_type_key);
        return owns;
    }

    if (auto record = context.records.find(source_type_key); record != context.records.end()) {
        for (auto const& field : record->second.fields) {
            if (is_owned_transfer_source_type_impl(field.source_type_name, context, visiting)) {
                visiting.erase(source_type_key);
                return true;
            }
        }
        visiting.erase(source_type_key);
        return false;
    }

    if (auto choice = context.choices.find(source_type_key); choice != context.choices.end()) {
        for (auto const& variant : choice->second.variants) {
            for (auto const& payload : variant.payloads) {
                if (is_owned_transfer_source_type_impl(payload.source_type_name, context, visiting)) {
                    visiting.erase(source_type_key);
                    return true;
                }
            }
        }
        visiting.erase(source_type_key);
        return false;
    }

    visiting.erase(source_type_key);
    return true;
}

}  // namespace

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

auto consumed_owned_binding_or_descendant_name(
    OwnershipTransferState const& state,
    std::string_view binding_name
) -> std::optional<std::string> {
    if (is_owned_binding_consumed(state, binding_name)) {
        return std::string {binding_name};
    }

    auto descendant_prefix = std::string {binding_name};
    descendant_prefix += ".";
    auto matches = std::vector<std::string> {};
    for (auto const& consumed_name : state.consumed_owned_bindings) {
        if (consumed_name.starts_with(descendant_prefix)) {
            matches.push_back(consumed_name);
        }
    }
    if (matches.empty()) {
        return std::nullopt;
    }
    std::ranges::sort(matches);
    return matches.front();
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

auto owned_binding_member_name(
    std::string_view owner_name,
    std::string_view member_name
) -> std::string {
    auto binding_name = std::string {owner_name};
    binding_name += ".";
    binding_name += member_name;
    return binding_name;
}

auto is_owned_transfer_source_type(
    std::string_view source_type_name,
    LoweringContext const& context
) -> bool {
    auto visiting = std::unordered_set<std::string> {};
    return is_owned_transfer_source_type_impl(source_type_name, context, visiting);
}

auto owned_record_field_transfer(
    std::string_view owner_name,
    std::string_view owner_source_type_name,
    std::string_view field_name,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer> {
    auto field_names = std::vector<std::string> {std::string {field_name}};
    return owned_record_member_path_transfer(owner_name, owner_source_type_name, field_names, context);
}

auto owned_record_member_path_transfer(
    std::string_view owner_name,
    std::string_view owner_source_type_name,
    std::span<std::string const> field_names,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer> {
    if (field_names.empty()) {
        return std::nullopt;
    }

    auto current_source_type_name = std::string {owner_source_type_name};
    auto member_name = std::string {};
    for (auto const& field_name : field_names) {
        auto record = context.records.find(current_source_type_name);
        if (record == context.records.end()) {
            return std::nullopt;
        }

        auto const* field = find_record_field(record->second, field_name);
        if (field == nullptr) {
            return std::nullopt;
        }

        if (!member_name.empty()) {
            member_name += ".";
        }
        member_name += field_name;
        current_source_type_name = field->source_type_name;
    }

    if (!is_owned_transfer_source_type(current_source_type_name, context)) {
        return std::nullopt;
    }

    return OwnedAggregateMemberTransfer {
        .binding_name = owned_binding_member_name(owner_name, member_name),
        .owner_name = std::string {owner_name},
        .member_name = std::move(member_name),
        .source_type_name = std::move(current_source_type_name),
    };
}

auto owned_choice_payload_transfer(
    std::string_view owner_name,
    std::string_view choice_source_type_name,
    std::string_view variant_name,
    std::string_view payload_name,
    LoweringContext const& context
) -> std::optional<OwnedAggregateMemberTransfer> {
    auto choice = context.choices.find(std::string {choice_source_type_name});
    if (choice == context.choices.end()) {
        return std::nullopt;
    }

    for (auto const& variant : choice->second.variants) {
        if (variant.name != variant_name) {
            continue;
        }
        for (auto const& payload : variant.payloads) {
            if (payload.name != payload_name ||
                !is_owned_transfer_source_type(payload.source_type_name, context)) {
                continue;
            }
            auto member_name = std::string {variant_name};
            member_name += ".";
            member_name += payload_name;
            return OwnedAggregateMemberTransfer {
                .binding_name = owned_binding_member_name(owner_name, member_name),
                .owner_name = std::string {owner_name},
                .member_name = std::move(member_name),
                .source_type_name = payload.source_type_name,
            };
        }
    }
    return std::nullopt;
}

}  // namespace orison::lowering
