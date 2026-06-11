#include "orison/lowering/llvm_names.hpp"

namespace orison::lowering {

auto llvm_local_value_name(std::string_view source_name) -> std::string {
    return "%" + std::string(source_name);
}

auto next_llvm_local_value_name(
    std::string_view source_name,
    std::unordered_map<std::string, std::size_t>& name_counts
) -> std::string {
    auto& count = name_counts[std::string(source_name)];
    auto value_name = llvm_local_value_name(source_name);
    if (count > 0) {
        value_name += "." + std::to_string(count);
    }
    ++count;
    return value_name;
}

auto next_llvm_temporary_name(std::size_t& next_index) -> std::string {
    return "%tmp" + std::to_string(next_index++);
}

auto next_llvm_block_index(std::size_t& next_index) -> std::size_t {
    return next_index++;
}

auto llvm_block_name(std::string_view prefix, std::size_t index) -> std::string {
    return std::string(prefix) + "." + std::to_string(index);
}

auto llvm_block_name(
    std::string_view prefix,
    std::size_t index,
    std::size_t variant
) -> std::string {
    return llvm_block_name(prefix, index) + "." + std::to_string(variant);
}

}  // namespace orison::lowering
