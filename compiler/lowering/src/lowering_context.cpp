#include "orison/lowering/lowering_context.hpp"

#include "orison/lowering/c_abi_adapter.hpp"
#include "orison/lowering/member_call_receiver.hpp"

#include <string_view>
#include <utility>

namespace orison::lowering {
namespace {

auto unquoted_text(std::string_view text) -> std::string_view {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

auto collect_method_signature(
    std::string receiver_type_name,
    syntax::FunctionSyntax const& method
) -> LoweredMethodSignature {
    auto symbol_name = lowered_method_symbol_name(receiver_type_name, method.name);
    return LoweredMethodSignature {
        .receiver_type_name = receiver_type_name,
        .method_name = method.name,
        .signature = lower_function_signature(
            method.return_type,
            method.parameters,
            std::move(symbol_name)
        ),
    };
}

}  // namespace

auto build_lowering_context(
    syntax::ModuleSyntax const& module,
    diagnostics::DiagnosticBag& diagnostics
) -> LoweringContext {
    auto context = LoweringContext {};
    for (auto const& function : module.functions) {
        auto signature =
            lower_function_signature(function.return_type, function.parameters, function.name);
        context.functions.emplace(function.name, std::move(signature));
    }

    for (auto const& implementation : module.implementations) {
        auto receiver_type_name = render_source_type_name(implementation.receiver_type);
        for (auto const& method : implementation.methods) {
            context.methods.push_back(collect_method_signature(receiver_type_name, method));
        }
    }

    for (auto const& extension : module.extensions) {
        auto receiver_type_name = render_source_type_name(extension.receiver_type);
        for (auto const& method : extension.methods) {
            context.methods.push_back(collect_method_signature(receiver_type_name, method));
        }
    }

    for (auto const& foreign_import : module.foreign_imports) {
        if (unquoted_text(foreign_import.abi) != "c") {
            continue;
        }
        for (auto const& function : foreign_import.functions) {
            auto symbol_name = function.external_name.empty()
                ? function.name
                : std::string(unquoted_text(function.external_name));
            auto const* adapter = find_c_abi_adapter(symbol_name);
            auto signature =
                lower_function_signature(function.return_type, function.parameters, std::move(symbol_name));
            if (adapter != nullptr) {
                auto validation = apply_c_abi_adapter(signature, *adapter);
                if (validation.error == CAbiAdapterValidationError::invalid_fixed_prefix) {
                    diagnostics.error(
                        1,
                        "foreign symbol '" + signature.symbol_name +
                            "' does not match the required fixed C ABI prefix"
                    );
                    continue;
                }
                if (validation.error == CAbiAdapterValidationError::unsupported_trailing_parameter) {
                    diagnostics.error(
                        1,
                        "foreign symbol '" + signature.symbol_name + "' parameter '" +
                            function.parameters[validation.parameter_index].name +
                            "' has no supported C variadic ABI representation"
                    );
                    continue;
                }
            }
            if (!has_supported_function_signature_types(signature)) {
                continue;
            }
            context.functions.emplace(function.name, signature);
            context.foreign_declarations.push_back(std::move(signature));
        }
    }
    return context;
}

auto find_lowered_method_signature(
    LoweringContext const& context,
    std::string_view receiver_type_name,
    std::string_view method_name
) -> LoweredMethodLookup {
    auto const* match = static_cast<LoweredMethodSignature const*>(nullptr);
    for (auto const& method : context.methods) {
        if (method.receiver_type_name != receiver_type_name || method.method_name != method_name) {
            continue;
        }
        if (match != nullptr) {
            return LoweredMethodLookup {
                .result = LoweredMethodLookupResult::ambiguous,
            };
        }
        match = &method;
    }

    if (match == nullptr) {
        return {};
    }
    return LoweredMethodLookup {
        .result = LoweredMethodLookupResult::found,
        .method = match,
    };
}

auto lowered_method_symbol_name(
    std::string_view receiver_type_name,
    std::string_view method_name
) -> std::string {
    auto symbol = std::string {"method."};
    auto append_sanitized = [&symbol](std::string_view text) {
        for (auto character : text) {
            if ((character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') ||
                character == '_') {
                symbol.push_back(character);
                continue;
            }
            symbol.push_back('_');
        }
    };
    append_sanitized(receiver_type_name);
    symbol += ".";
    append_sanitized(method_name);
    return symbol;
}

}  // namespace orison::lowering
