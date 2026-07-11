#include "orison/lowering/maybe_value_emitter.hpp"

#include "orison/lowering/llvm_names.hpp"
#include "orison/lowering/source_type_queries.hpp"

namespace orison::lowering {
namespace {

auto trim(std::string_view text) -> std::string_view {
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    return text;
}

auto maybe_payload_llvm_type(std::string_view llvm_type) -> std::optional<std::string> {
    llvm_type = trim(llvm_type);
    if (!llvm_type.starts_with("{") || !llvm_type.ends_with("}")) {
        return std::nullopt;
    }

    auto body = trim(llvm_type.substr(1, llvm_type.size() - 2));
    if (!body.starts_with("i1,")) {
        return std::nullopt;
    }

    auto payload = trim(body.substr(3));
    if (payload.empty()) {
        return std::nullopt;
    }
    return std::string(payload);
}

}  // namespace

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

auto maybe_value_abi_for_llvm_type(
    std::string_view llvm_type,
    LoweringContext const& context
) -> std::optional<MaybeValueAbi> {
    auto payload_llvm_type = maybe_payload_llvm_type(llvm_type);
    if (!payload_llvm_type.has_value() || *payload_llvm_type == "void") {
        return std::nullopt;
    }

    auto payload_source_type = source_type_name_for_llvm_type(*payload_llvm_type, context);
    return MaybeValueAbi {
        .source_type_name = payload_source_type.has_value()
            ? "Maybe<" + *payload_source_type + ">"
            : std::string {},
        .payload_source_type_name = payload_source_type.value_or(std::string {}),
        .llvm_type = std::string(llvm_type),
        .payload_llvm_type = std::move(*payload_llvm_type),
    };
}

auto emit_empty_maybe_value(
    MaybeValueAbi const& abi,
    std::size_t& next_temporary_index,
    std::ostringstream& output
) -> LoweredExpression {
    auto temporary_name = next_llvm_temporary_name(next_temporary_index);
    output << "  " << temporary_name << " = insertvalue " << abi.llvm_type
           << " undef, i1 false, 0\n";
    return LoweredExpression {
        .type = abi.llvm_type,
        .value = std::move(temporary_name),
        .signedness = IntegerSignedness::not_integer,
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

    return emit_empty_maybe_value(*abi, next_temporary_index, output);
}

auto emit_some_maybe_value(
    MaybeValueAbi const& abi,
    LoweredExpression const& payload,
    std::size_t& next_temporary_index,
    std::ostringstream& output
) -> std::optional<LoweredExpression> {
    if (payload.type != abi.payload_llvm_type) {
        return std::nullopt;
    }

    auto tagged_name = next_llvm_temporary_name(next_temporary_index);
    output << "  " << tagged_name << " = insertvalue " << abi.llvm_type
           << " undef, i1 true, 0\n";

    auto value_name = next_llvm_temporary_name(next_temporary_index);
    output << "  " << value_name << " = insertvalue " << abi.llvm_type << " "
           << tagged_name << ", " << abi.payload_llvm_type << " " << payload.value
           << ", 1\n";
    return LoweredExpression {
        .type = abi.llvm_type,
        .value = std::move(value_name),
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
    if (!abi.has_value()) {
        return std::nullopt;
    }

    return emit_some_maybe_value(*abi, payload, next_temporary_index, output);
}

}  // namespace orison::lowering
