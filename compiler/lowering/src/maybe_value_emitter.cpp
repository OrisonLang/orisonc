#include "orison/lowering/maybe_value_emitter.hpp"

#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/source_type_queries.hpp"

namespace orison::lowering {

auto maybe_value_abi_for_source_type(
    std::string_view source_type_name,
    LoweringContext const& context
) -> std::optional<MaybeValueAbi> {
    auto payload_source_type = maybe_payload_source_type_name(source_type_name);
    if (!payload_source_type.has_value()) {
        return std::nullopt;
    }

    auto maybe_llvm_type = llvm_type_for_source_type_name(source_type_name, context);
    auto payload_llvm_type = llvm_type_for_source_type_name(*payload_source_type, context);
    if (!maybe_llvm_type.has_value() || !payload_llvm_type.has_value() ||
        *maybe_llvm_type == "void" || *payload_llvm_type == "void") {
        return std::nullopt;
    }

    return MaybeValueAbi {
        .source_type_name = std::string(source_type_name),
        .payload_source_type_name = std::move(*payload_source_type),
        .llvm_type = std::move(*maybe_llvm_type),
        .payload_llvm_type = std::move(*payload_llvm_type),
    };
}

auto emit_empty_maybe_value(
    std::string_view source_type_name,
    LoweringContext const& context,
    std::size_t& next_temporary_index,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto abi = maybe_value_abi_for_source_type(source_type_name, context);
    if (!abi.has_value()) {
        return std::nullopt;
    }

    auto temporary_name = next_llvm_temporary_name(next_temporary_index);
    output << "  " << temporary_name << " = insertvalue " << abi->llvm_type
           << " undef, i1 false, 0\n";
    return LoweredExpression {
        .type = std::move(abi->llvm_type),
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

auto emit_some_maybe_value(
    std::string_view source_type_name,
    LoweredExpression const& payload,
    LoweringContext const& context,
    std::size_t& next_temporary_index,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    auto abi = maybe_value_abi_for_source_type(source_type_name, context);
    if (!abi.has_value() || payload.type != abi->payload_llvm_type) {
        return std::nullopt;
    }

    auto tagged_name = next_llvm_temporary_name(next_temporary_index);
    output << "  " << tagged_name << " = insertvalue " << abi->llvm_type
           << " undef, i1 true, 0\n";

    auto value_name = next_llvm_temporary_name(next_temporary_index);
    output << "  " << value_name << " = insertvalue " << abi->llvm_type << " "
           << tagged_name << ", " << abi->payload_llvm_type << " " << payload.value
           << ", 1\n";
    return LoweredExpression {
        .type = std::move(abi->llvm_type),
        .value = std::move(value_name),
        .signedness = IntegerSignedness::not_integer,
    };
}

}  // namespace orison::lowering
