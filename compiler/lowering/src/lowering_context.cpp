#include "orison/lowering/lowering_context.hpp"

#include "orison/lowering/c_abi_adapter.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <string_view>
#include <unordered_map>
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

auto contextual_record_type_for(
    syntax::TypeSyntax const& type,
    std::unordered_set<std::string> const& record_names
) -> std::optional<std::string> {
    if (type.generic_arguments.empty() && record_names.contains(type.name)) {
        return lowered_record_type_name(type.name);
    }
    return std::nullopt;
}

auto contextual_choice_type_for(
    syntax::TypeSyntax const& type,
    std::unordered_map<std::string, LoweredChoiceLayout> const& choices
) -> std::optional<std::string> {
    if (!type.generic_arguments.empty()) {
        return std::nullopt;
    }
    auto choice = choices.find(type.name);
    if (choice == choices.end() || choice->second.llvm_type_name.empty()) {
        return std::nullopt;
    }
    return choice->second.llvm_type_name;
}

auto lower_contextual_function_signature(
    syntax::TypeSyntax const& return_type,
    std::vector<syntax::ParameterSyntax> const& parameters,
    std::string symbol_name,
    std::unordered_set<std::string> const& record_names,
    std::unordered_map<std::string, LoweredChoiceLayout> const& choices
) -> LoweredFunctionSignature {
    auto signature = lower_function_signature(return_type, parameters, std::move(symbol_name));
    if (signature.return_type.empty()) {
        if (auto record_type = contextual_record_type_for(return_type, record_names)) {
            signature.return_type = std::move(*record_type);
            signature.return_signedness = IntegerSignedness::not_integer;
        } else if (auto choice_type = contextual_choice_type_for(return_type, choices)) {
            signature.return_type = std::move(*choice_type);
            signature.return_signedness = IntegerSignedness::not_integer;
        }
    }

    for (auto index = std::size_t {0}; index < parameters.size(); ++index) {
        if (!signature.parameter_types[index].empty()) {
            continue;
        }
        if (auto record_type = contextual_record_type_for(parameters[index].type, record_names)) {
            signature.parameter_types[index] = std::move(*record_type);
            signature.parameter_signedness[index] = IntegerSignedness::not_integer;
        } else if (auto choice_type = contextual_choice_type_for(parameters[index].type, choices)) {
            signature.parameter_types[index] = std::move(*choice_type);
            signature.parameter_signedness[index] = IntegerSignedness::not_integer;
        }
    }
    return signature;
}

auto lower_method_signature(
    syntax::TypeSyntax const& receiver_type,
    syntax::FunctionSyntax const& method,
    std::string symbol_name,
    std::unordered_set<std::string> const& record_names,
    std::unordered_map<std::string, LoweredChoiceLayout> const& choices
) -> LoweredFunctionSignature {
    auto parameters = method.parameters;
    for (auto& parameter : parameters) {
        if (parameter.name == "this" && is_receiver_self_type(parameter.type)) {
            parameter.type = receiver_type;
        }
    }
    return lower_contextual_function_signature(
        method.return_type,
        parameters,
        std::move(symbol_name),
        record_names,
        choices
    );
}

auto collect_method_signature(
    syntax::TypeSyntax const& receiver_type,
    std::string receiver_type_name,
    syntax::FunctionSyntax const& method,
    std::unordered_set<std::string> const& record_names,
    std::unordered_map<std::string, LoweredChoiceLayout> const& choices
) -> LoweredMethodSignature {
    auto symbol_name = lowered_method_symbol_name(receiver_type_name, method.name);
    return LoweredMethodSignature {
        .receiver_type_name = receiver_type_name,
        .method_name = method.name,
        .signature = lower_method_signature(receiver_type, method, std::move(symbol_name), record_names, choices),
    };
}

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

auto llvm_field_type_for(
    syntax::TypeSyntax const& type,
    std::unordered_set<std::string> const& record_names
) -> std::string {
    if (auto lowered_type = llvm_type_for(type); lowered_type.has_value()) {
        return std::string {*lowered_type};
    }
    if (type.generic_arguments.empty() && record_names.contains(type.name)) {
        return lowered_record_type_name(type.name);
    }
    if (type.name == "Array" && type.generic_arguments.size() == 2 &&
        is_decimal_integer_text(type.generic_arguments[1].name)) {
        auto element_type = llvm_field_type_for(type.generic_arguments[0], record_names);
        if (!element_type.empty() && element_type != "void") {
            return "[" + type.generic_arguments[1].name + " x " + element_type + "]";
        }
    }
    if (type.name == "Maybe" && type.generic_arguments.size() == 1) {
        auto payload_type = llvm_field_type_for(type.generic_arguments[0], record_names);
        if (!payload_type.empty() && payload_type != "void") {
            return "{ i1, " + payload_type + " }";
        }
    }
    return {};
}

auto is_supported_choice_payload_llvm_type(std::string_view type) -> bool {
    return type == "i1" ||
           type == "i8" ||
           type == "i16" ||
           type == "i32" ||
           type == "i64" ||
           type == "float" ||
           type == "double";
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
        layout.fields.push_back(LoweredRecordField {
            .name = field.name,
            .source_type_name = render_source_type_name(field.type),
            .llvm_type = llvm_field_type_for(field.type, record_names),
            .index = index,
        });
    }
    return layout;
}

auto collect_choice_layout(
    syntax::ChoiceSyntax const& choice,
    std::unordered_set<std::string> const& record_names
) -> LoweredChoiceLayout {
    auto choice_type = syntax::TypeSyntax {
        .name = choice.name,
    };
    for (auto const& generic_parameter : choice.generic_parameters) {
        choice_type.generic_arguments.push_back(syntax::TypeSyntax {
            .name = generic_parameter,
        });
    }

    auto layout = LoweredChoiceLayout {
        .name = choice.name,
        .source_type_name = render_source_type_name(choice_type),
        .generic_parameters = choice.generic_parameters,
    };
    layout.variants.reserve(choice.variants.size());
    auto payload_llvm_type = std::optional<std::string> {};
    auto supports_scalar_payload_abi = choice.generic_parameters.empty();
    if (!supports_scalar_payload_abi) {
        layout.unsupported_abi_reason = "generic choices do not yet have a lowered choice ABI";
    }
    for (auto variant_index = std::size_t {0}; variant_index < choice.variants.size(); ++variant_index) {
        auto const& variant = choice.variants[variant_index];
        auto lowered_variant = LoweredChoiceVariant {
            .name = variant.name,
            .tag = variant_index,
        };
        if (variant.payloads.size() > 1) {
            supports_scalar_payload_abi = false;
            if (layout.unsupported_abi_reason.empty()) {
                layout.unsupported_abi_reason = "variants with multiple payloads do not yet have a lowered choice ABI";
            }
        }
        lowered_variant.payloads.reserve(variant.payloads.size());
        for (auto payload_index = std::size_t {0}; payload_index < variant.payloads.size(); ++payload_index) {
            auto const& payload = variant.payloads[payload_index];
            auto payload_llvm = llvm_field_type_for(payload.type, record_names);
            if (!is_supported_choice_payload_llvm_type(payload_llvm)) {
                supports_scalar_payload_abi = false;
                if (layout.unsupported_abi_reason.empty()) {
                    layout.unsupported_abi_reason =
                        "choice payload type '" + render_source_type_name(payload.type) +
                        "' does not yet have a scalar lowered choice ABI";
                }
            } else if (!payload_llvm_type.has_value()) {
                payload_llvm_type = payload_llvm;
            } else if (*payload_llvm_type != payload_llvm) {
                supports_scalar_payload_abi = false;
                if (layout.unsupported_abi_reason.empty()) {
                    layout.unsupported_abi_reason =
                        "choice payload variants must share one scalar LLVM payload type";
                }
            }
            lowered_variant.payloads.push_back(LoweredChoicePayload {
                .name = payload.name,
                .source_type_name = render_source_type_name(payload.type),
                .llvm_type = std::move(payload_llvm),
                .index = payload_index,
            });
        }
        layout.variants.push_back(std::move(lowered_variant));
    }
    if (supports_scalar_payload_abi && payload_llvm_type.has_value()) {
        layout.llvm_type_name = "{ i32, " + *payload_llvm_type + " }";
        layout.unsupported_abi_reason.clear();
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
    for (auto const& choice : module.choices) {
        context.choices.emplace(choice.name, collect_choice_layout(choice, record_names));
    }

    for (auto const& function : module.functions) {
        auto signature = lower_contextual_function_signature(
            function.return_type,
            function.parameters,
            function.name,
            record_names,
            context.choices
        );
        context.functions.emplace(function.name, std::move(signature));
    }

    for (auto const& implementation : module.implementations) {
        auto receiver_type_name = render_source_type_name(implementation.receiver_type);
        for (auto const& method : implementation.methods) {
            context.methods.push_back(collect_method_signature(
                    implementation.receiver_type,
                    receiver_type_name,
                    method,
                    record_names,
                    context.choices
            ));
        }
    }

    for (auto const& extension : module.extensions) {
        auto receiver_type_name = render_source_type_name(extension.receiver_type);
        for (auto const& method : extension.methods) {
            context.methods.push_back(collect_method_signature(
                    extension.receiver_type,
                    receiver_type_name,
                    method,
                    record_names,
                    context.choices
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

auto find_lowered_choice_layout_by_llvm_type(
    LoweringContext const& context,
    std::string_view llvm_type
) -> LoweredChoiceLayout const* {
    auto const* match = static_cast<LoweredChoiceLayout const*>(nullptr);
    for (auto const& [choice_name, layout] : context.choices) {
        (void)choice_name;
        if (layout.llvm_type_name != llvm_type) {
            continue;
        }
        if (match != nullptr) {
            return nullptr;
        }
        match = &layout;
    }
    return match;
}

}  // namespace orison::lowering
