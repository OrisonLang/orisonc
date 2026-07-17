#include "orison/lowering/target_layout.hpp"

#include "orison/lowering/lowering_context.hpp"

#include <algorithm>
#include <charconv>
#include <string_view>
#include <vector>

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

auto parse_literal_struct_type(std::string_view llvm_type) -> std::optional<std::vector<std::string_view>> {
    if (llvm_type.size() < 4 || llvm_type.front() != '{' || llvm_type.back() != '}') {
        return std::nullopt;
    }

    auto fields = std::vector<std::string_view> {};
    auto inner = llvm_type.substr(1, llvm_type.size() - 2);
    auto depth = std::size_t {0};
    auto start = std::size_t {0};
    for (auto index = std::size_t {0}; index < inner.size(); ++index) {
        auto const character = inner[index];
        if (character == '{' || character == '[') {
            ++depth;
            continue;
        }
        if (character == '}' || character == ']') {
            if (depth > 0) {
                --depth;
            }
            continue;
        }
        if (character != ',' || depth != 0) {
            continue;
        }

        auto field = inner.substr(start, index - start);
        while (!field.empty() && field.front() == ' ') {
            field.remove_prefix(1);
        }
        while (!field.empty() && field.back() == ' ') {
            field.remove_suffix(1);
        }
        if (field.empty()) {
            return std::nullopt;
        }
        fields.push_back(field);
        start = index + 1;
    }

    auto field = inner.substr(start);
    while (!field.empty() && field.front() == ' ') {
        field.remove_prefix(1);
    }
    while (!field.empty() && field.back() == ' ') {
        field.remove_suffix(1);
    }
    if (field.empty()) {
        return std::nullopt;
    }
    fields.push_back(field);
    return fields;
}

auto record_layout_for(
    std::string_view llvm_type,
    LoweringContext const* context
) -> LoweredRecordLayout const* {
    if (context == nullptr) {
        return nullptr;
    }
    for (auto const& [name, record] : context->records) {
        (void)name;
        if (record.llvm_type_name == llvm_type) {
            return &record;
        }
    }
    return nullptr;
}

auto lowered_type_layout(
    std::string_view llvm_type,
    TargetLayout const& layout,
    LoweringContext const* context,
    std::vector<std::string_view>& active_records
) -> std::optional<SizeAndAlignment>;

auto lowered_struct_layout(
    std::vector<std::string_view> const& field_types,
    TargetLayout const& layout,
    LoweringContext const* context,
    std::vector<std::string_view>& active_records
) -> std::optional<SizeAndAlignment> {
    auto offset = std::size_t {0};
    auto struct_alignment = std::size_t {1};

    for (auto field_type : field_types) {
        auto field_layout = lowered_type_layout(field_type, layout, context, active_records);
        if (!field_layout.has_value()) {
            return std::nullopt;
        }
        offset = align_to(offset, field_layout->alignment_bytes);
        offset += field_layout->size_bytes;
        struct_alignment = std::max(struct_alignment, field_layout->alignment_bytes);
    }

    return SizeAndAlignment {
        .size_bytes = align_to(offset, struct_alignment),
        .alignment_bytes = struct_alignment,
    };
}

auto lowered_type_layout(
    std::string_view llvm_type,
    TargetLayout const& layout,
    LoweringContext const* context,
    std::vector<std::string_view>& active_records
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
        auto element = lowered_type_layout(array->second, layout, context, active_records);
        if (!element.has_value()) {
            return std::nullopt;
        }
        return SizeAndAlignment {
            .size_bytes = element->size_bytes * array->first,
            .alignment_bytes = element->alignment_bytes,
        };
    }
    if (auto literal_struct = parse_literal_struct_type(llvm_type); literal_struct.has_value()) {
        return lowered_struct_layout(*literal_struct, layout, context, active_records);
    }
    if (auto const* record = record_layout_for(llvm_type, context); record != nullptr) {
        if (std::find(active_records.begin(), active_records.end(), llvm_type) != active_records.end()) {
            return std::nullopt;
        }
        active_records.push_back(llvm_type);
        auto field_types = std::vector<std::string_view> {};
        field_types.reserve(record->fields.size());
        for (auto const& field : record->fields) {
            field_types.push_back(field.llvm_type);
        }
        auto record_layout = lowered_struct_layout(field_types, layout, context, active_records);
        active_records.pop_back();
        return record_layout;
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
    auto active_records = std::vector<std::string_view> {};
    auto type_layout = lowered_type_layout(llvm_type, layout, nullptr, active_records);
    if (!type_layout.has_value()) {
        return std::nullopt;
    }
    return type_layout->size_bytes;
}

auto lowered_type_size_bytes(
    std::string_view llvm_type,
    LoweringContext const& context,
    TargetLayout const& layout
) -> std::optional<std::size_t> {
    auto active_records = std::vector<std::string_view> {};
    auto type_layout = lowered_type_layout(llvm_type, layout, &context, active_records);
    if (!type_layout.has_value()) {
        return std::nullopt;
    }
    return type_layout->size_bytes;
}

auto lowered_struct_size_bytes(
    std::vector<std::string_view> const& field_types,
    TargetLayout const& layout
) -> std::optional<std::size_t> {
    auto active_records = std::vector<std::string_view> {};
    auto struct_layout = lowered_struct_layout(field_types, layout, nullptr, active_records);
    if (!struct_layout.has_value()) {
        return std::nullopt;
    }
    return struct_layout->size_bytes;
}

auto lowered_struct_size_bytes(
    std::vector<std::string_view> const& field_types,
    LoweringContext const& context,
    TargetLayout const& layout
) -> std::optional<std::size_t> {
    auto active_records = std::vector<std::string_view> {};
    auto struct_layout = lowered_struct_layout(field_types, layout, &context, active_records);
    if (!struct_layout.has_value()) {
        return std::nullopt;
    }
    return struct_layout->size_bytes;
}

}  // namespace orison::lowering
