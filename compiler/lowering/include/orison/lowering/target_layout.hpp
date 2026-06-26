#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace orison::lowering {

struct LoweringContext;

struct TargetLayout {
    std::size_t pointer_size_bytes = 8;
    std::size_t pointer_alignment_bytes = 8;
};

auto native_target_layout() -> TargetLayout;

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
