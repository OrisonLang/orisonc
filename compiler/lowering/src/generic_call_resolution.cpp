#include "orison/lowering/generic_call_resolution.hpp"

#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/source_type_queries.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace orison::lowering {
namespace {

auto parse_source_type_name(std::string_view type_name) -> syntax::TypeSyntax {
    auto depth = std::size_t {0};
    auto generic_start = std::optional<std::size_t> {};
    for (auto index = std::size_t {0}; index < type_name.size(); ++index) {
        auto const character = type_name[index];
        if (character == '<') {
            if (depth == 0 && !generic_start.has_value()) {
                generic_start = index;
            }
            ++depth;
            continue;
        }
        if (character == '>' && depth > 0) {
            --depth;
        }
    }
    if (!generic_start.has_value() || !type_name.ends_with(">")) {
        return syntax::TypeSyntax {.name = std::string {type_name}};
    }

    auto parsed = syntax::TypeSyntax {
        .name = std::string {type_name.substr(0, *generic_start)},
    };
    auto argument_start = *generic_start + 1;
    depth = 0;
    for (auto index = argument_start; index + 1 < type_name.size(); ++index) {
        auto const character = type_name[index];
        if (character == '<') {
            ++depth;
            continue;
        }
        if (character == '>' && depth > 0) {
            --depth;
            continue;
        }
        if (character == ',' && depth == 0) {
            auto argument = type_name.substr(argument_start, index - argument_start);
            while (!argument.empty() && argument.front() == ' ') {
                argument.remove_prefix(1);
            }
            while (!argument.empty() && argument.back() == ' ') {
                argument.remove_suffix(1);
            }
            parsed.generic_arguments.push_back(parse_source_type_name(argument));
            argument_start = index + 1;
        }
    }

    auto argument = type_name.substr(argument_start, type_name.size() - argument_start - 1);
    while (!argument.empty() && argument.front() == ' ') {
        argument.remove_prefix(1);
    }
    while (!argument.empty() && argument.back() == ' ') {
        argument.remove_suffix(1);
    }
    if (!argument.empty()) {
        parsed.generic_arguments.push_back(parse_source_type_name(argument));
    }
    return parsed;
}

auto substitute_type(
    syntax::TypeSyntax const& type,
    std::unordered_map<std::string, syntax::TypeSyntax> const& substitutions
) -> syntax::TypeSyntax {
    if (type.generic_arguments.empty()) {
        auto substitution = substitutions.find(type.name);
        if (substitution != substitutions.end()) {
            return substitution->second;
        }
        return type;
    }

    auto substituted = syntax::TypeSyntax {.name = type.name};
    substituted.generic_arguments.reserve(type.generic_arguments.size());
    for (auto const& argument : type.generic_arguments) {
        substituted.generic_arguments.push_back(substitute_type(argument, substitutions));
    }
    return substituted;
}

auto generic_parameter_set(std::vector<std::string> const& generic_parameters) -> std::unordered_set<std::string> {
    auto parameters = std::unordered_set<std::string> {};
    for (auto const& parameter : generic_parameters) {
        parameters.insert(parameter);
    }
    return parameters;
}

auto unify_generic_type(
    syntax::TypeSyntax const& pattern,
    syntax::TypeSyntax const& actual,
    std::unordered_set<std::string> const& generic_parameters,
    std::unordered_map<std::string, syntax::TypeSyntax>& substitutions
) -> bool {
    if (pattern.generic_arguments.empty() && generic_parameters.contains(pattern.name)) {
        auto existing = substitutions.find(pattern.name);
        if (existing == substitutions.end()) {
            substitutions.emplace(pattern.name, actual);
            return true;
        }
        return render_source_type_name(existing->second) == render_source_type_name(actual);
    }

    if (pattern.name != actual.name || pattern.generic_arguments.size() != actual.generic_arguments.size()) {
        return false;
    }
    for (auto index = std::size_t {0}; index < pattern.generic_arguments.size(); ++index) {
        if (!unify_generic_type(
                pattern.generic_arguments[index],
                actual.generic_arguments[index],
                generic_parameters,
                substitutions
            )) {
            return false;
        }
    }
    return true;
}

}  // namespace

auto generic_specialization_base_name(std::string_view symbol_name) -> std::optional<std::string> {
    auto delimiter = symbol_name.find("__");
    if (delimiter == std::string_view::npos) {
        return std::nullopt;
    }
    return std::string {symbol_name.substr(0, delimiter)};
}

auto source_type_name_for_generic_call_argument(
    syntax::ExpressionSyntax const& expression,
    GenericCallSourceResolver const& resolver
) -> std::optional<std::string> {
    if (resolver.lowering_context != nullptr && resolver.state != nullptr) {
        if (auto source_type = source_type_name_for_expression(expression, *resolver.lowering_context, *resolver.state)) {
            return source_type;
        }
    }

    if (expression.kind == syntax::ExpressionKind::name) {
        if (resolver.local_source_types == nullptr) {
            return std::nullopt;
        }
        auto source_type = resolver.local_source_types->find(expression.text);
        if (source_type != resolver.local_source_types->end()) {
            return source_type->second;
        }
        return std::nullopt;
    }

    if (expression.kind != syntax::ExpressionKind::call ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    if (resolver.generic_functions != nullptr) {
        auto generic_function = resolver.generic_functions->find(expression.left->text);
        if (generic_function != resolver.generic_functions->end()) {
            auto substitutions = bind_generic_function_call_substitutions(
                *generic_function->second,
                expression,
                resolver
            );
            if (!substitutions.has_value()) {
                return std::nullopt;
            }
            return render_source_type_name(substitute_type(generic_function->second->return_type, *substitutions));
        }
    }

    if (resolver.functions != nullptr) {
        auto source_function = resolver.functions->find(expression.left->text);
        if (source_function != resolver.functions->end() &&
            !source_function->second.source_return_type_name.empty()) {
            return source_function->second.source_return_type_name;
        }
    }
    return std::nullopt;
}

auto bind_generic_function_call_substitutions(
    syntax::FunctionSyntax const& function,
    syntax::ExpressionSyntax const& call,
    GenericCallSourceResolver const& resolver
) -> std::optional<std::unordered_map<std::string, syntax::TypeSyntax>> {
    if (function.generic_parameters.empty() || call.arguments.size() != function.parameters.size()) {
        return std::nullopt;
    }

    auto generic_parameters = generic_parameter_set(function.generic_parameters);
    auto substitutions = std::unordered_map<std::string, syntax::TypeSyntax> {};
    for (auto index = std::size_t {0}; index < call.arguments.size(); ++index) {
        auto source_type_name = source_type_name_for_generic_call_argument(call.arguments[index], resolver);
        if (!source_type_name.has_value()) {
            return std::nullopt;
        }
        if (!unify_generic_type(
                function.parameters[index].type,
                parse_source_type_name(*source_type_name),
                generic_parameters,
                substitutions
            )) {
            return std::nullopt;
        }
    }

    for (auto const& generic_parameter : function.generic_parameters) {
        if (!substitutions.contains(generic_parameter)) {
            return std::nullopt;
        }
    }
    return substitutions;
}

auto bind_generic_method_call_substitutions(
    syntax::TypeSyntax const& receiver_type,
    syntax::FunctionSyntax const& method,
    syntax::ExpressionSyntax const& call,
    GenericCallSourceResolver const& resolver
) -> std::optional<std::unordered_map<std::string, syntax::TypeSyntax>> {
    if (method.generic_parameters.empty() ||
        call.left == nullptr ||
        call.left->left == nullptr ||
        call.arguments.size() + 1 != method.parameters.size()) {
        return std::nullopt;
    }

    auto actual_receiver_type = source_type_name_for_generic_call_argument(*call.left->left, resolver);
    if (!actual_receiver_type.has_value()) {
        return std::nullopt;
    }

    auto generic_parameters = generic_parameter_set(method.generic_parameters);
    auto substitutions = std::unordered_map<std::string, syntax::TypeSyntax> {};
    if (!unify_generic_type(
            receiver_type,
            parse_source_type_name(*actual_receiver_type),
            generic_parameters,
            substitutions
        )) {
        return std::nullopt;
    }

    for (auto index = std::size_t {0}; index < call.arguments.size(); ++index) {
        auto source_type_name = source_type_name_for_generic_call_argument(call.arguments[index], resolver);
        if (!source_type_name.has_value()) {
            return std::nullopt;
        }
        if (!unify_generic_type(
                method.parameters[index + 1].type,
                parse_source_type_name(*source_type_name),
                generic_parameters,
                substitutions
            )) {
            return std::nullopt;
        }
    }

    for (auto const& generic_parameter : method.generic_parameters) {
        if (!substitutions.contains(generic_parameter)) {
            return std::nullopt;
        }
    }
    return substitutions;
}

auto call_arguments_match_source_types(
    syntax::ExpressionSyntax const& expression,
    LoweredFunctionSignature const& signature,
    GenericCallSourceResolver const& resolver
) -> bool {
    if (signature.parameter_source_type_names.size() != expression.arguments.size()) {
        return false;
    }

    for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
        auto const& expected_source_type = signature.parameter_source_type_names[index];
        if (expected_source_type.empty()) {
            return false;
        }
        auto actual_source_type = source_type_name_for_generic_call_argument(expression.arguments[index], resolver);
        if (!actual_source_type.has_value() || *actual_source_type != expected_source_type) {
            return false;
        }
    }
    return true;
}

auto method_call_arguments_match_source_types(
    syntax::ExpressionSyntax const& expression,
    LoweredFunctionSignature const& signature,
    GenericCallSourceResolver const& resolver
) -> bool {
    if (signature.parameter_source_type_names.size() != expression.arguments.size() + 1) {
        return false;
    }

    for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
        auto const& expected_source_type = signature.parameter_source_type_names[index + 1];
        if (expected_source_type.empty()) {
            return false;
        }
        auto actual_source_type = source_type_name_for_generic_call_argument(expression.arguments[index], resolver);
        if (!actual_source_type.has_value() || *actual_source_type != expected_source_type) {
            return false;
        }
    }
    return true;
}

auto find_matching_generic_specialization(
    std::string_view function_name,
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> LoweredFunctionSignature const* {
    auto resolver = GenericCallSourceResolver {
        .lowering_context = &context,
        .state = &state,
    };
    auto const* match = static_cast<LoweredFunctionSignature const*>(nullptr);
    for (auto const& specialization : context.generic_function_specializations) {
        auto base_name = generic_specialization_base_name(specialization->name);
        if (!base_name.has_value() || *base_name != function_name) {
            continue;
        }
        auto signature = context.functions.find(specialization->name);
        if (signature == context.functions.end() ||
            signature->second.return_type != expected_llvm_type ||
            signature->second.parameter_types.size() != expression.arguments.size() ||
            !call_arguments_match_source_types(expression, signature->second, resolver)) {
            continue;
        }
        if (match != nullptr) {
            return nullptr;
        }
        match = &signature->second;
    }
    return match;
}

auto find_matching_generic_method_specialization(
    std::string_view receiver_type_name,
    std::string_view method_name,
    syntax::ExpressionSyntax const& expression,
    std::string_view expected_llvm_type,
    LoweringContext const& context,
    FunctionLoweringState const& state
) -> LoweredFunctionSignature const* {
    auto resolver = GenericCallSourceResolver {
        .lowering_context = &context,
        .state = &state,
    };
    auto const* match = static_cast<LoweredFunctionSignature const*>(nullptr);
    for (auto const& method : context.methods) {
        if (method.receiver_type_name != receiver_type_name ||
            method.method_name != method_name ||
            method.signature.return_type != expected_llvm_type ||
            method.signature.parameter_types.size() != expression.arguments.size() + 1 ||
            method.signature.parameter_source_type_names.empty() ||
            method.signature.parameter_source_type_names.front() != receiver_type_name ||
            !method_call_arguments_match_source_types(expression, method.signature, resolver)) {
            continue;
        }
        if (match != nullptr) {
            return nullptr;
        }
        match = &method.signature;
    }
    return match;
}

}  // namespace orison::lowering
