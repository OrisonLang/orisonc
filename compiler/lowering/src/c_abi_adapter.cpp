#include "orison/lowering/c_abi_adapter.hpp"

#include <array>

namespace orison::lowering {
namespace {

constexpr auto adapters = std::array {
    CAbiAdapterMetadata {
        .symbol_name = "printf",
        .return_type = "i32",
        .first_parameter_type = "ptr",
        .fixed_parameter_count = 1,
        .kind = CAbiAdapterKind::variadic,
    },
};

auto is_supported_variadic_type(std::string_view llvm_type) -> bool {
    return llvm_type == "ptr" || llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16" ||
           llvm_type == "i32" || llvm_type == "i64" || llvm_type == "float" || llvm_type == "double";
}

}  // namespace

auto find_c_abi_adapter(std::string_view symbol_name) -> CAbiAdapterMetadata const* {
    for (auto const& adapter : adapters) {
        if (adapter.symbol_name == symbol_name) {
            return &adapter;
        }
    }
    return nullptr;
}

auto validate_c_abi_adapter_signature(
    CAbiAdapterMetadata const& adapter,
    std::string_view return_type,
    std::span<std::string const> parameter_types
) -> CAbiAdapterValidationResult {
    if (return_type != adapter.return_type || parameter_types.size() < adapter.fixed_parameter_count ||
        parameter_types.empty() ||
        std::string_view(parameter_types.front()) != adapter.first_parameter_type) {
        return {.error = CAbiAdapterValidationError::invalid_fixed_prefix};
    }

    for (auto index = adapter.fixed_parameter_count; index < parameter_types.size(); ++index) {
        if (!is_supported_variadic_type(std::string_view(parameter_types[index]))) {
            return {
                .error = CAbiAdapterValidationError::unsupported_trailing_parameter,
                .parameter_index = index,
            };
        }
    }
    return {};
}

auto c_abi_promotion_for(std::string_view llvm_type) -> CAbiPromotion {
    if (llvm_type == "i1" || llvm_type == "i8" || llvm_type == "i16") {
        return CAbiPromotion::integer_to_i32;
    }
    if (llvm_type == "float") {
        return CAbiPromotion::float_to_double;
    }
    return CAbiPromotion::none;
}

}  // namespace orison::lowering
