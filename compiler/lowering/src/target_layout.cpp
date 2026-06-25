#include "orison/lowering/target_layout.hpp"

#include <algorithm>
#include <charconv>
#include <string_view>

namespace orison::lowering {
namespace {

struct SizeAndAlignment {
    std::size_t size_bytes = 0;
    std::size_t alignment_bytes = 1;
};

auto align_to(std::size_t value, std::size_t alignment) -> std::size_t {
    if (alignment <= 1) {
        return value;
    }
    auto const remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
}

auto parse_decimal_size(std::string_view text) -> std::optional<std::size_t> {
    auto value = std::size_t {};
    auto const* begin = text.data();
    auto const* end = text.data() + text.size();
    auto [ptr, error] = std::from_chars(begin, end, value);
    if (error != std::errc {} || ptr != end) {
        return std::nullopt;
    }
    return value;
}

auto integer_type_size(std::string_view llvm_type) -> std::optional<SizeAndAlignment> {
    if (llvm_type.size() < 2 || llvm_type.front() != 'i') {
        return std::nullopt;
    }

    auto bits = parse_decimal_size(llvm_type.substr(1));
    if (!bits.has_value() || *bits == 0) {
        return std::nullopt;
    }

    auto size = (*bits + 7) / 8;
    return SizeAndAlignment {
        .size_bytes = size,
        .alignment_bytes = std::max<std::size_t>(1, size),
    };
}

auto parse_array_type(std::string_view llvm_type) -> std::optional<std::pair<std::size_t, std::string_view>> {
    if (llvm_type.size() < 6 || llvm_type.front() != '[' || llvm_type.back() != ']') {
        return std::nullopt;
    }

    auto inner = llvm_type.substr(1, llvm_type.size() - 2);
    auto separator = inner.find(" x ");
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    auto count = parse_decimal_size(inner.substr(0, separator));
    if (!count.has_value()) {
        return std::nullopt;
    }
    return std::pair {*count, inner.substr(separator + 3)};
}

auto lowered_type_layout(
    std::string_view llvm_type,
    TargetLayout const& layout
) -> std::optional<SizeAndAlignment> {
    if (auto integer = integer_type_size(llvm_type); integer.has_value()) {
        return integer;
    }
    if (llvm_type == "ptr") {
        return SizeAndAlignment {
            .size_bytes = layout.pointer_size_bytes,
            .alignment_bytes = layout.pointer_alignment_bytes,
        };
    }
    if (llvm_type == "float") {
        return SizeAndAlignment {.size_bytes = 4, .alignment_bytes = 4};
    }
    if (llvm_type == "double") {
        return SizeAndAlignment {.size_bytes = 8, .alignment_bytes = 8};
    }
    if (auto array = parse_array_type(llvm_type); array.has_value()) {
        auto element = lowered_type_layout(array->second, layout);
        if (!element.has_value()) {
            return std::nullopt;
        }
        return SizeAndAlignment {
            .size_bytes = element->size_bytes * array->first,
            .alignment_bytes = element->alignment_bytes,
        };
    }
    return std::nullopt;
}

}  // namespace

auto native_target_layout() -> TargetLayout {
    return TargetLayout {};
}

auto lowered_type_size_bytes(
    std::string_view llvm_type,
    TargetLayout const& layout
) -> std::optional<std::size_t> {
    auto type_layout = lowered_type_layout(llvm_type, layout);
    if (!type_layout.has_value()) {
        return std::nullopt;
    }
    return type_layout->size_bytes;
}

auto lowered_struct_size_bytes(
    std::vector<std::string_view> const& field_types,
    TargetLayout const& layout
) -> std::optional<std::size_t> {
    auto offset = std::size_t {0};
    auto struct_alignment = std::size_t {1};

    for (auto field_type : field_types) {
        auto field_layout = lowered_type_layout(field_type, layout);
        if (!field_layout.has_value()) {
            return std::nullopt;
        }
        offset = align_to(offset, field_layout->alignment_bytes);
        offset += field_layout->size_bytes;
        struct_alignment = std::max(struct_alignment, field_layout->alignment_bytes);
    }

    return align_to(offset, struct_alignment);
}

}  // namespace orison::lowering
