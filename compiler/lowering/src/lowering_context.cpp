#include "orison/lowering/lowering_context.hpp"

#include "orison/lowering/c_abi_adapter.hpp"

#include <cstddef>
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

auto render_type_name(syntax::TypeSyntax const& type) -> std::string {
    auto rendered = type.name;
    if (type.generic_arguments.empty()) {
        return rendered;
    }

    rendered += "<";
    for (auto index = std::size_t {0}; index < type.generic_arguments.size(); ++index) {
        if (index > 0) {
            rendered += ", ";
        }
        rendered += render_type_name(type.generic_arguments[index]);
    }
    rendered += ">";
    return rendered;
}

auto collect_method_signature(
    std::string receiver_type_name,
    syntax::FunctionSyntax const& method
) -> LoweredMethodSignature {
    return LoweredMethodSignature {
        .receiver_type_name = std::move(receiver_type_name),
        .method_name = method.name,
        .signature = lower_function_signature(method.return_type, method.parameters, method.name),
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
        auto receiver_type_name = render_type_name(implementation.receiver_type);
        for (auto const& method : implementation.methods) {
            context.methods.push_back(collect_method_signature(receiver_type_name, method));
        }
    }

    for (auto const& extension : module.extensions) {
        auto receiver_type_name = render_type_name(extension.receiver_type);
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

}  // namespace orison::lowering
