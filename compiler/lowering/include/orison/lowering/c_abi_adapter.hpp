#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

namespace orison::lowering {

enum class CAbiAdapterKind {
    none,
    variadic,
};

enum class CAbiPromotion {
    none,
    integer_to_i32,
    float_to_double,
};

struct CAbiAdapterMetadata {
    std::string_view symbol_name;
    std::string_view return_type;
    std::string_view first_parameter_type;
    std::size_t fixed_parameter_count;
    CAbiAdapterKind kind;
};

enum class CAbiAdapterValidationError {
    none,
    invalid_fixed_prefix,
    unsupported_trailing_parameter,
};

struct CAbiAdapterValidationResult {
    CAbiAdapterValidationError error = CAbiAdapterValidationError::none;
    std::size_t parameter_index = 0;
};

auto find_c_abi_adapter(std::string_view symbol_name) -> CAbiAdapterMetadata const*;

auto validate_c_abi_adapter_signature(
    CAbiAdapterMetadata const& adapter,
    std::string_view return_type,
    std::span<std::string const> parameter_types
) -> CAbiAdapterValidationResult;

auto c_abi_promotion_for(std::string_view llvm_type) -> CAbiPromotion;

}  // namespace orison::lowering
