#include "orison/lowering/function_signature.hpp"

#include <utility>

namespace orison::lowering {

auto lower_function_signature(
    syntax::TypeSyntax const& return_type,
    std::vector<syntax::ParameterSyntax> const& parameters,
    std::string symbol_name
) -> LoweredFunctionSignature {
    auto signature = LoweredFunctionSignature {
        .return_signedness = integer_signedness_for(return_type),
        .parameter_signedness = parameter_signedness_for(parameters),
        .symbol_name = std::move(symbol_name),
    };

    if (auto lowered_return_type = llvm_type_for(return_type)) {
        signature.return_type = *lowered_return_type;
    }
    signature.parameter_types.reserve(parameters.size());
    for (auto const& parameter : parameters) {
        auto lowered_parameter_type = llvm_type_for(parameter.type);
        signature.parameter_types.push_back(
            lowered_parameter_type.has_value() ? std::string(*lowered_parameter_type) : std::string {}
        );
    }
    return signature;
}

auto has_supported_function_signature_types(LoweredFunctionSignature const& signature) -> bool {
    if (signature.return_type.empty()) {
        return false;
    }
    for (auto const& parameter_type : signature.parameter_types) {
        if (parameter_type.empty() || parameter_type == "void") {
            return false;
        }
    }
    return true;
}

auto apply_c_abi_adapter(
    LoweredFunctionSignature& signature,
    CAbiAdapterMetadata const& adapter
) -> CAbiAdapterValidationResult {
    auto validation =
        validate_c_abi_adapter_signature(adapter, signature.return_type, signature.parameter_types);
    if (validation.error == CAbiAdapterValidationError::none) {
        signature.adapter = adapter.kind;
        signature.fixed_abi_parameter_count = adapter.fixed_parameter_count;
    }
    return validation;
}

}  // namespace orison::lowering
