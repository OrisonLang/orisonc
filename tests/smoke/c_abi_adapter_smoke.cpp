#include "orison/lowering/c_abi_adapter.hpp"

#include <array>
#include <cassert>
#include <string>

int main() {
    auto const* printf_adapter = orison::lowering::find_c_abi_adapter("printf");
    assert(printf_adapter != nullptr);
    assert(printf_adapter->kind == orison::lowering::CAbiAdapterKind::variadic);
    assert(orison::lowering::find_c_abi_adapter("strcmp") == nullptr);

    auto valid_types = std::array<std::string, 5> {"ptr", "i16", "i64", "float", "double"};
    auto valid = orison::lowering::validate_c_abi_adapter_signature(*printf_adapter, "i32", valid_types);
    assert(valid.error == orison::lowering::CAbiAdapterValidationError::none);

    auto invalid_prefix_types = std::array<std::string, 1> {"i32"};
    auto invalid_prefix =
        orison::lowering::validate_c_abi_adapter_signature(*printf_adapter, "i32", invalid_prefix_types);
    assert(invalid_prefix.error == orison::lowering::CAbiAdapterValidationError::invalid_fixed_prefix);

    auto unsupported_types = std::array<std::string, 2> {"ptr", ""};
    auto unsupported =
        orison::lowering::validate_c_abi_adapter_signature(*printf_adapter, "i32", unsupported_types);
    assert(unsupported.error == orison::lowering::CAbiAdapterValidationError::unsupported_trailing_parameter);
    assert(unsupported.parameter_index == 1);

    assert(orison::lowering::c_abi_promotion_for("i16") ==
           orison::lowering::CAbiPromotion::integer_to_i32);
    assert(orison::lowering::c_abi_promotion_for("float") ==
           orison::lowering::CAbiPromotion::float_to_double);
    assert(orison::lowering::c_abi_promotion_for("i64") == orison::lowering::CAbiPromotion::none);
    return 0;
}
