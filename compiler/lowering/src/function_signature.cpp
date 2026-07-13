#include "orison/lowering/function_signature.hpp"

#include "orison/lowering/member_call_receiver.hpp"

#include <utility>

namespace orison::lowering {
namespace {

auto is_decimal_integer_text(std::string_view text) -> bool {
    if (text.empty()) {
        return false;
    }
    for (auto character : text) {
        if (character < '0' || character > '9') {
            return false;
        }
    }
    return true;
}

auto lowered_function_type_for(syntax::TypeSyntax const& type) -> std::optional<std::string> {
    if (type.name == "Array" && type.generic_arguments.size() == 2 &&
        is_decimal_integer_text(type.generic_arguments[1].name)) {
        auto element_type = lowered_function_type_for(type.generic_arguments[0]);
        if (element_type.has_value() && *element_type != "void") {
            return "[" + type.generic_arguments[1].name + " x " + *element_type + "]";
        }
        return std::nullopt;
    }

    if ((type.name == "View" || type.name == "shared.View" || type.name == "exclusive.View") &&
        type.generic_arguments.size() == 1) {
        auto element_type = lowered_function_type_for(type.generic_arguments[0]);
        return element_type.has_value() && *element_type != "void"
            ? std::optional<std::string> {"ptr"}
            : std::nullopt;
    }

    if (auto lowered_type = llvm_type_for(type); lowered_type.has_value()) {
        return std::string {*lowered_type};
    }

    return std::nullopt;
}

}  // namespace

auto lower_function_signature(
    syntax::TypeSyntax const& return_type,
    std::vector<syntax::ParameterSyntax> const& parameters,
    std::string symbol_name
) -> LoweredFunctionSignature {
    auto signature = LoweredFunctionSignature {
        .source_return_type_name = render_source_type_name(return_type),
        .return_signedness = integer_signedness_for(return_type),
        .parameter_signedness = parameter_signedness_for(parameters),
        .symbol_name = std::move(symbol_name),
    };

    if (auto lowered_return_type = lowered_function_type_for(return_type)) {
        signature.return_type = std::move(*lowered_return_type);
    }
    signature.parameter_types.reserve(parameters.size());
    signature.parameter_source_type_names.reserve(parameters.size());
    for (auto const& parameter : parameters) {
        auto lowered_parameter_type = lowered_function_type_for(parameter.type);
        signature.parameter_types.push_back(lowered_parameter_type.has_value()
                                                ? std::move(*lowered_parameter_type)
                                                : std::string {});
        signature.parameter_source_type_names.push_back(render_source_type_name(parameter.type));
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
