#pragma once

#include <bit>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct LoweringContext;

enum class TargetByteOrder {
    little_endian,
    big_endian,
};

struct TargetLayout {
    std::size_t pointer_size_bytes = 8;
    std::size_t pointer_alignment_bytes = 8;
    TargetByteOrder byte_order = std::endian::native == std::endian::big
        ? TargetByteOrder::big_endian
        : TargetByteOrder::little_endian;
};

struct LoweredTypeLayout {
    std::size_t size_bytes = 0;
    std::size_t alignment_bytes = 1;
};

struct LoweredStructLayout {
    std::size_t size_bytes = 0;
    std::size_t alignment_bytes = 1;
    std::vector<std::size_t> field_offsets;
};

auto native_target_layout() -> TargetLayout;

auto lowered_type_layout_bytes(
    std::string_view llvm_type,
    TargetLayout const& layout = native_target_layout()
) -> std::optional<LoweredTypeLayout>;

auto lowered_type_layout_bytes(
    std::string_view llvm_type,
    LoweringContext const& context,
    TargetLayout const& layout = native_target_layout()
) -> std::optional<LoweredTypeLayout>;

auto lowered_struct_layout_bytes(
    std::vector<std::string_view> const& field_types,
    TargetLayout const& layout = native_target_layout()
) -> std::optional<LoweredStructLayout>;

auto lowered_struct_layout_bytes(
    std::vector<std::string_view> const& field_types,
    LoweringContext const& context,
    TargetLayout const& layout = native_target_layout()
) -> std::optional<LoweredStructLayout>;

auto lowered_type_size_bytes(
    std::string_view llvm_type,
    TargetLayout const& layout = native_target_layout()
) -> std::optional<std::size_t>;

auto lowered_type_size_bytes(
    std::string_view llvm_type,
    LoweringContext const& context,
    TargetLayout const& layout = native_target_layout()
) -> std::optional<std::size_t>;

auto lowered_struct_size_bytes(
    std::vector<std::string_view> const& field_types,
    TargetLayout const& layout = native_target_layout()
) -> std::optional<std::size_t>;

auto lowered_struct_size_bytes(
    std::vector<std::string_view> const& field_types,
    LoweringContext const& context,
    TargetLayout const& layout = native_target_layout()
) -> std::optional<std::size_t>;

}  // namespace orison::lowering
