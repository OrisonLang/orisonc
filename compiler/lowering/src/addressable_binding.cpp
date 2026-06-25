#include "orison/lowering/addressable_binding.hpp"

#include "orison/lowering/llvm_names.hpp"

#include <ostream>
#include <string>
#include <utility>

namespace orison::lowering {

auto is_aggregate_llvm_type(std::string_view type) -> bool {
    return type.starts_with("%record.") || type.starts_with("[");
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

}  // namespace orison::lowering
