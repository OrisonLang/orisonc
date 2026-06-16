#include "orison/lowering/lowering_context.hpp"

#include "orison/lowering/c_abi_adapter.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <string_view>
#include <unordered_set>
#include <utility>

namespace orison::lowering {
namespace {

auto unquoted_text(std::string_view text) -> std::string_view {
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"') {
        return text.substr(1, text.size() - 2);
    }
    return text;
}

auto is_receiver_self_type(syntax::TypeSyntax const& type) -> bool {
    return type.generic_arguments.empty() &&
        (type.name == "This" || type.name == "shared.This" || type.name == "exclusive.This");
}

auto lower_method_signature(
    syntax::TypeSyntax const& receiver_type,
    syntax::FunctionSyntax const& method,
    std::string symbol_name
) -> LoweredFunctionSignature {
    auto parameters = method.parameters;
    for (auto& parameter : parameters) {
        if (parameter.name == "this" && is_receiver_self_type(parameter.type)) {
            parameter.type = receiver_type;
        }
    }
    return lower_function_signature(method.return_type, parameters, std::move(symbol_name));
}

auto collect_method_signature(
    syntax::TypeSyntax const& receiver_type,
    std::string receiver_type_name,
    syntax::FunctionSyntax const& method
) -> LoweredMethodSignature {
    auto symbol_name = lowered_method_symbol_name(receiver_type_name, method.name);
    return LoweredMethodSignature {
        .receiver_type_name = receiver_type_name,
        .method_name = method.name,
        .signature = lower_method_signature(receiver_type, method, std::move(symbol_name)),
    };
}

auto collect_record_layout(
    syntax::RecordSyntax const& record,
    std::unordered_set<std::string> const& record_names
) -> LoweredRecordLayout {
    auto layout = LoweredRecordLayout {
        .name = record.name,
        .llvm_type_name = lowered_record_type_name(record.name),
    };
    layout.fields.reserve(record.fields.size());
    for (auto index = std::size_t {0}; index < record.fields.size(); ++index) {
        auto const& field = record.fields[index];
        auto llvm_type = std::string {};
        if (auto lowered_type = llvm_type_for(field.type); lowered_type.has_value()) {
            llvm_type = std::string {*lowered_type};
        } else if (field.type.generic_arguments.empty() && record_names.contains(field.type.name)) {
            llvm_type = lowered_record_type_name(field.type.name);
        }
        layout.fields.push_back(LoweredRecordField {
            .name = field.name,
            .source_type_name = render_source_type_name(field.type),
            .llvm_type = std::move(llvm_type),
            .index = index,
        });
    }
    return layout;
}

}  // namespace

auto build_lowering_context(
    syntax::ModuleSyntax const& module,
    diagnostics::DiagnosticBag& diagnostics
) -> LoweringContext {
    auto context = LoweringContext {};
    auto record_names = std::unordered_set<std::string> {};
    for (auto const& record : module.records) {
        record_names.insert(record.name);
    }
    for (auto const& record : module.records) {
        context.records.emplace(record.name, collect_record_layout(record, record_names));
    }

    for (auto const& function : module.functions) {
        auto signature =
            lower_function_signature(function.return_type, function.parameters, function.name);
        context.functions.emplace(function.name, std::move(signature));
    }

    for (auto const& implementation : module.implementations) {
        auto receiver_type_name = render_source_type_name(implementation.receiver_type);
        for (auto const& method : implementation.methods) {
            context.methods.push_back(collect_method_signature(
                implementation.receiver_type,
                receiver_type_name,
                method
            ));
        }
    }

    for (auto const& extension : module.extensions) {
        auto receiver_type_name = render_source_type_name(extension.receiver_type);
        for (auto const& method : extension.methods) {
            context.methods.push_back(collect_method_signature(
                extension.receiver_type,
                receiver_type_name,
                method
            ));
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

auto lowered_record_type_name(std::string_view record_name) -> std::string {
    auto type_name = std::string {"%record."};
    for (auto character : record_name) {
        if ((character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') ||
            character == '_') {
            type_name.push_back(character);
            continue;
        }
        type_name.push_back('_');
    }
    return type_name;
}

}  // namespace orison::lowering
