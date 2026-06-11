#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

namespace orison::lowering {

auto llvm_local_value_name(std::string_view source_name) -> std::string;

auto next_llvm_local_value_name(
    std::string_view source_name,
    std::unordered_map<std::string, std::size_t>& name_counts
) -> std::string;

auto next_llvm_temporary_name(std::size_t& next_index) -> std::string;

auto next_llvm_block_index(std::size_t& next_index) -> std::size_t;

auto llvm_block_name(std::string_view prefix, std::size_t index) -> std::string;

auto llvm_block_name(
    std::string_view prefix,
    std::size_t index,
    std::size_t variant
) -> std::string;

}  // namespace orison::lowering
