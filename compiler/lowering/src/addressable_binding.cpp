#include "orison/lowering/addressable_binding.hpp"

#include "orison/lowering/llvm_names.hpp"

#include <ostream>
#include <optional>
#include <string>
#include <utility>

namespace orison::lowering {

auto is_aggregate_llvm_type(std::string_view type) -> bool {
    return type.starts_with("%record.") || type.starts_with("[");
}

auto aggregate_storage_for_name(
    std::string_view name,
    FunctionLoweringState const& state
) -> std::optional<std::string> {
    auto const key = std::string(name);
    if (auto binding = state.mutable_bindings.find(key); binding != state.mutable_bindings.end()) {
        return binding->second.storage;
    }
    if (auto binding = state.addressable_bindings.find(key);
        binding != state.addressable_bindings.end()) {
        return binding->second.storage;
    }
    return std::nullopt;
}

void bind_addressable_aggregate_value(
    std::string_view name,
    LoweredExpression const& lowered,
    FunctionLoweringSession& session,
    std::ostream& output
) {
    if (!is_aggregate_llvm_type(lowered.type)) {
        return;
    }

    auto storage = next_llvm_local_value_name(
        std::string(name) + ".addr",
        session.state.local_name_counts
    );
    output << "  " << storage << " = alloca " << lowered.type << "\n";
    output << "  store " << lowered.type << " " << lowered.value << ", ptr " << storage << "\n";
    session.state.addressable_bindings[std::string(name)] = AddressableBinding {
        .type = LoweredType {
            .type = lowered.type,
            .signedness = lowered.signedness,
        },
        .storage = std::move(storage),
    };
}

auto spill_aggregate_value_to_temporary_storage(
    LoweredExpression const& lowered,
    FunctionLoweringSession& session,
    std::ostream& output
) -> std::optional<std::string> {
    if (!is_aggregate_llvm_type(lowered.type)) {
        return std::nullopt;
    }

    auto storage = next_llvm_temporary_name(session.state.next_temporary_index);
    output << "  " << storage << " = alloca " << lowered.type << "\n";
    output << "  store " << lowered.type << " " << lowered.value << ", ptr " << storage << "\n";
    return storage;
}

}  // namespace orison::lowering
