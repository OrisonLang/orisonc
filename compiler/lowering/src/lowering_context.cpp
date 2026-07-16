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

auto render_record_type_name(syntax::TypeSyntax const& type) -> std::string {
    return render_source_type_name(type);
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

auto generic_parameter_set(syntax::RecordSyntax const& record) -> std::unordered_set<std::string> {
    auto parameters = std::unordered_set<std::string> {};
    for (auto const& parameter : record.generic_parameters) {
        parameters.insert(parameter);
    }
    return parameters;
}

auto unify_constructor_type(
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
        if (!unify_constructor_type(
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

auto infer_constructor_expression_type(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records
) -> std::optional<syntax::TypeSyntax>;

auto infer_constructor_argument_type(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records
) -> std::optional<syntax::TypeSyntax> {
    if (expression.kind == syntax::ExpressionKind::cast && !expression.text.empty()) {
        return parse_source_type_name(expression.text);
    }
    if (expression.kind == syntax::ExpressionKind::call) {
        return infer_constructor_expression_type(expression, generic_records);
    }
    if (expression.kind == syntax::ExpressionKind::array_literal && !expression.arguments.empty()) {
        auto element_type = infer_constructor_argument_type(expression.arguments.front(), generic_records);
        if (!element_type.has_value()) {
            return std::nullopt;
        }
        for (auto index = std::size_t {1}; index < expression.arguments.size(); ++index) {
            auto next_element_type = infer_constructor_argument_type(expression.arguments[index], generic_records);
            if (!next_element_type.has_value() ||
                render_source_type_name(*next_element_type) != render_source_type_name(*element_type)) {
                return std::nullopt;
            }
        }
        return syntax::TypeSyntax {
            .name = "Array",
            .generic_arguments = {
                *element_type,
                syntax::TypeSyntax {.name = std::to_string(expression.arguments.size())},
            },
        };
    }
    return std::nullopt;
}

auto infer_constructor_expression_type(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records
) -> std::optional<syntax::TypeSyntax> {
    if (expression.kind != syntax::ExpressionKind::call || expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto record = generic_records.find(expression.left->text);
    if (record == generic_records.end() || record->second->fields.size() != expression.arguments.size()) {
        return std::nullopt;
    }

    auto substitutions = std::unordered_map<std::string, syntax::TypeSyntax> {};
    auto parameters = generic_parameter_set(*record->second);
    for (auto index = std::size_t {0}; index < expression.arguments.size(); ++index) {
        auto argument_type = infer_constructor_argument_type(expression.arguments[index], generic_records);
        if (!argument_type.has_value() ||
            !unify_constructor_type(
                record->second->fields[index].type,
                *argument_type,
                parameters,
                substitutions
            )) {
            return std::nullopt;
        }
    }

    auto concrete_type = syntax::TypeSyntax {.name = record->second->name};
    concrete_type.generic_arguments.reserve(record->second->generic_parameters.size());
    for (auto const& parameter : record->second->generic_parameters) {
        auto substitution = substitutions.find(parameter);
        if (substitution == substitutions.end()) {
            return std::nullopt;
        }
        concrete_type.generic_arguments.push_back(substitution->second);
    }
    return concrete_type;
}

void collect_expression_type_instantiations(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
);

void collect_statement_type_instantiations(
    syntax::StatementSyntax const& statement,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
);

void collect_type_instantiations(
    syntax::TypeSyntax const& type,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
) {
    for (auto const& argument : type.generic_arguments) {
        collect_type_instantiations(argument, generic_records, pending, seen);
    }

    auto record = generic_records.find(type.name);
    if (record == generic_records.end() ||
        type.generic_arguments.size() != record->second->generic_parameters.size()) {
        return;
    }

    auto source_type_name = render_record_type_name(type);
    if (seen.insert(source_type_name).second) {
        pending.push_back(type);
    }
}

void collect_expression_type_instantiations(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
) {
    if (auto inferred_type = infer_constructor_expression_type(expression, generic_records)) {
        collect_type_instantiations(*inferred_type, generic_records, pending, seen);
    }

    for (auto const& argument : expression.arguments) {
        collect_expression_type_instantiations(argument, generic_records, pending, seen);
    }
    if (expression.left != nullptr) {
        collect_expression_type_instantiations(*expression.left, generic_records, pending, seen);
    }
    if (expression.right != nullptr) {
        collect_expression_type_instantiations(*expression.right, generic_records, pending, seen);
    }
    if (expression.alternate != nullptr) {
        collect_expression_type_instantiations(*expression.alternate, generic_records, pending, seen);
    }
    for (auto const& statement : expression.nested_statements) {
        if (statement != nullptr) {
            collect_statement_type_instantiations(*statement, generic_records, pending, seen);
        }
    }
}

void collect_statement_type_instantiations(
    syntax::StatementSyntax const& statement,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
) {
    if (!statement.annotated_type.name.empty()) {
        collect_type_instantiations(statement.annotated_type, generic_records, pending, seen);
    }
    collect_expression_type_instantiations(statement.assignment_target, generic_records, pending, seen);
    collect_expression_type_instantiations(statement.expression, generic_records, pending, seen);
    for (auto const& nested_statement : statement.nested_statements) {
        collect_statement_type_instantiations(nested_statement, generic_records, pending, seen);
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        collect_statement_type_instantiations(alternate_statement, generic_records, pending, seen);
    }
    for (auto const& switch_case : statement.switch_cases) {
        collect_expression_type_instantiations(switch_case.pattern, generic_records, pending, seen);
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr) {
                collect_statement_type_instantiations(*case_statement, generic_records, pending, seen);
            }
        }
    }
}

void collect_function_type_instantiations(
    syntax::FunctionSyntax const& function,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
) {
    collect_type_instantiations(function.return_type, generic_records, pending, seen);
    for (auto const& parameter : function.parameters) {
        collect_type_instantiations(parameter.type, generic_records, pending, seen);
    }
    for (auto const& constraint : function.where_constraints) {
        for (auto const& requirement : constraint.requirements) {
            collect_type_instantiations(requirement, generic_records, pending, seen);
        }
    }
    for (auto const& statement : function.body_statements) {
        collect_statement_type_instantiations(statement, generic_records, pending, seen);
    }
}

auto collect_generic_record_instantiations(
    syntax::ModuleSyntax const& module,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records
) -> std::vector<syntax::TypeSyntax> {
    auto pending = std::vector<syntax::TypeSyntax> {};
    auto seen = std::unordered_set<std::string> {};

    for (auto const& alias : module.type_aliases) {
        collect_type_instantiations(alias.aliased_type, generic_records, pending, seen);
    }
    for (auto const& constant : module.constants) {
        collect_type_instantiations(constant.type, generic_records, pending, seen);
    }
    for (auto const& record : module.records) {
        for (auto const& field : record.fields) {
            collect_type_instantiations(field.type, generic_records, pending, seen);
        }
    }
    for (auto const& choice : module.choices) {
        for (auto const& variant : choice.variants) {
            for (auto const& payload : variant.payloads) {
                collect_type_instantiations(payload.type, generic_records, pending, seen);
            }
        }
    }
    for (auto const& interface : module.interfaces) {
        for (auto const& method : interface.methods) {
            collect_type_instantiations(method.return_type, generic_records, pending, seen);
            for (auto const& parameter : method.parameters) {
                collect_type_instantiations(parameter.type, generic_records, pending, seen);
            }
            for (auto const& constraint : method.where_constraints) {
                for (auto const& requirement : constraint.requirements) {
                    collect_type_instantiations(requirement, generic_records, pending, seen);
                }
            }
        }
    }
    for (auto const& foreign_import : module.foreign_imports) {
        for (auto const& function : foreign_import.functions) {
            collect_type_instantiations(function.return_type, generic_records, pending, seen);
            for (auto const& parameter : function.parameters) {
                collect_type_instantiations(parameter.type, generic_records, pending, seen);
            }
        }
    }
    for (auto const& foreign_export : module.foreign_exports) {
        collect_function_type_instantiations(foreign_export.function, generic_records, pending, seen);
    }
    for (auto const& implementation : module.implementations) {
        collect_type_instantiations(implementation.interface_type, generic_records, pending, seen);
        collect_type_instantiations(implementation.receiver_type, generic_records, pending, seen);
        for (auto const& method : implementation.methods) {
            collect_function_type_instantiations(method, generic_records, pending, seen);
        }
    }
    for (auto const& extension : module.extensions) {
        collect_type_instantiations(extension.receiver_type, generic_records, pending, seen);
        for (auto const& method : extension.methods) {
            collect_function_type_instantiations(method, generic_records, pending, seen);
        }
    }
    for (auto const& function : module.functions) {
        collect_function_type_instantiations(function, generic_records, pending, seen);
    }

    for (auto index = std::size_t {0}; index < pending.size(); ++index) {
        auto record = generic_records.find(pending[index].name);
        if (record == generic_records.end()) {
            continue;
        }

        auto substitutions = std::unordered_map<std::string, syntax::TypeSyntax> {};
        for (auto argument_index = std::size_t {0}; argument_index < pending[index].generic_arguments.size();
             ++argument_index) {
            substitutions.emplace(
                record->second->generic_parameters[argument_index],
                pending[index].generic_arguments[argument_index]
            );
        }
        for (auto const& field : record->second->fields) {
            collect_type_instantiations(
                substitute_type(field.type, substitutions),
                generic_records,
                pending,
                seen
            );
        }
    }

    return pending;
}

auto contextual_record_type_for(
    syntax::TypeSyntax const& type,
    std::unordered_set<std::string> const& record_names
) -> std::optional<std::string> {
    auto source_type_name = render_record_type_name(type);
    if (record_names.contains(source_type_name)) {
        return lowered_record_type_name(source_type_name);
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

auto is_decimal_integer_text(std::string_view text) -> bool;

auto contextual_type_for(
    syntax::TypeSyntax const& type,
    std::unordered_set<std::string> const& record_names,
    std::unordered_map<std::string, LoweredChoiceLayout> const& choices
) -> std::optional<std::string> {
    if (auto record_type = contextual_record_type_for(type, record_names)) {
        return record_type;
    }
    if (auto choice_type = contextual_choice_type_for(type, choices)) {
        return choice_type;
    }
    if (type.name == "Maybe" && type.generic_arguments.size() == 1) {
        auto payload_type = contextual_type_for(type.generic_arguments.front(), record_names, choices);
        if (payload_type.has_value()) {
            return "{ i1, " + *payload_type + " }";
        }
    }
    if (type.name == "Array" && type.generic_arguments.size() == 2 &&
        is_decimal_integer_text(type.generic_arguments[1].name)) {
        auto element_type = contextual_type_for(type.generic_arguments[0], record_names, choices);
        if (element_type.has_value() && *element_type != "void") {
            return "[" + type.generic_arguments[1].name + " x " + *element_type + "]";
        }
    }
    return std::nullopt;
}

auto lower_contextual_function_signature(
    syntax::TypeSyntax const& return_type,
    std::vector<syntax::ParameterSyntax> const& parameters,
    std::string symbol_name,
    std::unordered_set<std::string> const& record_names,
    std::unordered_map<std::string, LoweredChoiceLayout> const& choices
) -> LoweredFunctionSignature {
    auto signature = lower_function_signature(return_type, parameters, std::move(symbol_name));
    signature.source_return_type_name = render_source_type_name(return_type);
    if (signature.return_type.empty()) {
        if (auto contextual_type = contextual_type_for(return_type, record_names, choices)) {
            signature.return_type = std::move(*contextual_type);
            signature.return_signedness = IntegerSignedness::not_integer;
        }
    }

    for (auto index = std::size_t {0}; index < parameters.size(); ++index) {
        if (!signature.parameter_types[index].empty()) {
            continue;
        }
        if (auto contextual_type = contextual_type_for(parameters[index].type, record_names, choices)) {
            signature.parameter_types[index] = std::move(*contextual_type);
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
    std::unordered_set<std::string> const& record_names,
    std::unordered_map<std::string, LoweredChoiceLayout> const* choices = nullptr
) -> std::string {
    if (auto lowered_type = llvm_type_for(type); lowered_type.has_value()) {
        return std::string {*lowered_type};
    }
    auto source_type_name = render_record_type_name(type);
    if (record_names.contains(source_type_name)) {
        return lowered_record_type_name(source_type_name);
    }
    if (type.generic_arguments.empty() && choices != nullptr) {
        auto choice = choices->find(type.name);
        if (choice != choices->end() && !choice->second.llvm_type_name.empty()) {
            return choice->second.llvm_type_name;
        }
    }
    if (type.name == "Array" && type.generic_arguments.size() == 2 &&
        is_decimal_integer_text(type.generic_arguments[1].name)) {
        auto element_type = llvm_field_type_for(type.generic_arguments[0], record_names, choices);
        if (!element_type.empty() && element_type != "void") {
            return "[" + type.generic_arguments[1].name + " x " + element_type + "]";
        }
    }
    if (type.name == "Maybe" && type.generic_arguments.size() == 1) {
        auto payload_type = llvm_field_type_for(type.generic_arguments[0], record_names, choices);
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
    std::unordered_set<std::string> const& record_names,
    std::unordered_map<std::string, LoweredChoiceLayout> const& choices
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
            .llvm_type = llvm_field_type_for(field.type, record_names, &choices),
            .index = index,
        });
    }
    return layout;
}

auto collect_instantiated_record_layout(
    syntax::TypeSyntax const& concrete_type,
    syntax::RecordSyntax const& record,
    std::unordered_set<std::string> const& record_names,
    std::unordered_map<std::string, LoweredChoiceLayout> const& choices
) -> LoweredRecordLayout {
    auto source_type_name = render_record_type_name(concrete_type);
    auto layout = LoweredRecordLayout {
        .name = source_type_name,
        .llvm_type_name = lowered_record_type_name(source_type_name),
    };
    auto substitutions = std::unordered_map<std::string, syntax::TypeSyntax> {};
    for (auto index = std::size_t {0}; index < record.generic_parameters.size(); ++index) {
        substitutions.emplace(record.generic_parameters[index], concrete_type.generic_arguments[index]);
    }

    layout.fields.reserve(record.fields.size());
    for (auto index = std::size_t {0}; index < record.fields.size(); ++index) {
        auto const& field = record.fields[index];
        auto substituted_type = substitute_type(field.type, substitutions);
        layout.fields.push_back(LoweredRecordField {
            .name = field.name,
            .source_type_name = render_source_type_name(substituted_type),
            .llvm_type = llvm_field_type_for(substituted_type, record_names, &choices),
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
    auto has_payload = false;
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
            has_payload = true;
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
    if (supports_scalar_payload_abi) {
        layout.llvm_type_name = has_payload ? "{ i32, " + *payload_llvm_type + " }" : "i32";
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
    auto generic_records = std::unordered_map<std::string, syntax::RecordSyntax const*> {};
    for (auto const& record : module.records) {
        record_names.insert(record.name);
        if (!record.generic_parameters.empty()) {
            generic_records.emplace(record.name, &record);
        }
    }
    auto instantiated_record_types = collect_generic_record_instantiations(module, generic_records);
    for (auto const& type : instantiated_record_types) {
        record_names.insert(render_record_type_name(type));
    }
    for (auto const& choice : module.choices) {
        context.choices.emplace(choice.name, collect_choice_layout(choice, record_names));
    }
    for (auto const& record : module.records) {
        context.records.emplace(record.name, collect_record_layout(record, record_names, context.choices));
    }
    for (auto const& type : instantiated_record_types) {
        auto record = generic_records.find(type.name);
        if (record == generic_records.end()) {
            continue;
        }
        auto source_type_name = render_record_type_name(type);
        context.records.emplace(
            source_type_name,
            collect_instantiated_record_layout(type, *record->second, record_names, context.choices)
        );
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
