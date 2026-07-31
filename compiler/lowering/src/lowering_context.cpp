#include "orison/lowering/lowering_context.hpp"

#include "orison/lowering/c_abi_adapter.hpp"
#include "orison/lowering/member_call_receiver.hpp"
#include "orison/lowering/source_type_queries.hpp"
#include "orison/lowering/target_layout.hpp"
#include "orison/lowering/type_lowering.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace orison::lowering {
namespace {

auto clone_expression(syntax::ExpressionSyntax const& expression) -> syntax::ExpressionSyntax;
auto clone_statement(syntax::StatementSyntax const& statement) -> syntax::StatementSyntax;
void collect_generic_calls_from_expression(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::FunctionSyntax const*> const& generic_functions,
    std::unordered_map<std::string, LoweredFunctionSignature> const& functions,
    std::unordered_map<std::string, std::string> const& local_source_types,
    std::vector<std::shared_ptr<syntax::FunctionSyntax>>& specializations
);
void collect_generic_calls_from_statement(
    syntax::StatementSyntax const& statement,
    std::unordered_map<std::string, syntax::FunctionSyntax const*> const& generic_functions,
    std::unordered_map<std::string, LoweredFunctionSignature> const& functions,
    std::unordered_map<std::string, std::string> const& local_source_types,
    std::vector<std::shared_ptr<syntax::FunctionSyntax>>& specializations
);

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

auto generic_parameter_set(syntax::ChoiceSyntax const& choice) -> std::unordered_set<std::string> {
    auto parameters = std::unordered_set<std::string> {};
    for (auto const& parameter : choice.generic_parameters) {
        parameters.insert(parameter);
    }
    return parameters;
}

auto generic_parameter_set(std::vector<std::string> const& generic_parameters) -> std::unordered_set<std::string> {
    auto parameters = std::unordered_set<std::string> {};
    for (auto const& parameter : generic_parameters) {
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

auto clone_expression(syntax::ExpressionSyntax const& expression) -> syntax::ExpressionSyntax {
    auto cloned = syntax::ExpressionSyntax {
        .kind = expression.kind,
        .line = expression.line,
        .text = expression.text,
    };
    cloned.arguments.reserve(expression.arguments.size());
    for (auto const& argument : expression.arguments) {
        cloned.arguments.push_back(clone_expression(argument));
    }
    cloned.nested_statements.reserve(expression.nested_statements.size());
    for (auto const& statement : expression.nested_statements) {
        cloned.nested_statements.push_back(std::make_unique<syntax::StatementSyntax>(clone_statement(*statement)));
    }
    if (expression.left != nullptr) {
        cloned.left = std::make_unique<syntax::ExpressionSyntax>(clone_expression(*expression.left));
    }
    if (expression.right != nullptr) {
        cloned.right = std::make_unique<syntax::ExpressionSyntax>(clone_expression(*expression.right));
    }
    if (expression.alternate != nullptr) {
        cloned.alternate = std::make_unique<syntax::ExpressionSyntax>(clone_expression(*expression.alternate));
    }
    return cloned;
}

auto clone_switch_case(syntax::SwitchCaseSyntax const& switch_case) -> syntax::SwitchCaseSyntax {
    auto cloned = syntax::SwitchCaseSyntax {
        .is_default = switch_case.is_default,
        .pattern = clone_expression(switch_case.pattern),
    };
    cloned.statements.reserve(switch_case.statements.size());
    for (auto const& statement : switch_case.statements) {
        cloned.statements.push_back(std::make_unique<syntax::StatementSyntax>(clone_statement(*statement)));
    }
    return cloned;
}

auto clone_statement(syntax::StatementSyntax const& statement) -> syntax::StatementSyntax {
    auto cloned = syntax::StatementSyntax {
        .kind = statement.kind,
        .line = statement.line,
        .valid = statement.valid,
        .name = statement.name,
        .annotated_type = statement.annotated_type,
        .assignment_target = clone_expression(statement.assignment_target),
        .assignment_operator = statement.assignment_operator,
        .expression = clone_expression(statement.expression),
    };
    cloned.nested_statements.reserve(statement.nested_statements.size());
    for (auto const& nested_statement : statement.nested_statements) {
        cloned.nested_statements.push_back(clone_statement(nested_statement));
    }
    cloned.alternate_statements.reserve(statement.alternate_statements.size());
    for (auto const& alternate_statement : statement.alternate_statements) {
        cloned.alternate_statements.push_back(clone_statement(alternate_statement));
    }
    cloned.switch_cases.reserve(statement.switch_cases.size());
    for (auto const& switch_case : statement.switch_cases) {
        cloned.switch_cases.push_back(clone_switch_case(switch_case));
    }
    return cloned;
}

auto clone_function(syntax::FunctionSyntax const& function) -> syntax::FunctionSyntax {
    auto cloned = syntax::FunctionSyntax {
        .visibility = function.visibility,
        .line = function.line,
        .is_async = function.is_async,
        .is_unsafe = function.is_unsafe,
        .name = function.name,
        .generic_parameters = function.generic_parameters,
        .parameters = function.parameters,
        .return_type = function.return_type,
        .where_constraints = function.where_constraints,
    };
    cloned.body_statements.reserve(function.body_statements.size());
    for (auto const& statement : function.body_statements) {
        cloned.body_statements.push_back(clone_statement(statement));
    }
    return cloned;
}

auto sanitized_specialization_part(std::string source_type_name) -> std::string {
    for (auto& character : source_type_name) {
        if (!std::isalnum(static_cast<unsigned char>(character))) {
            character = '_';
        }
    }
    return source_type_name;
}

auto specialized_function_symbol_name(
    syntax::FunctionSyntax const& function,
    std::unordered_map<std::string, syntax::TypeSyntax> const& substitutions
) -> std::string {
    auto symbol = function.name;
    for (auto const& generic_parameter : function.generic_parameters) {
        auto substitution = substitutions.find(generic_parameter);
        if (substitution == substitutions.end()) {
            return function.name;
        }
        symbol += "__";
        symbol += sanitized_specialization_part(render_source_type_name(substitution->second));
    }
    return symbol;
}

auto source_type_name_for_generic_specialization_argument(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::FunctionSyntax const*> const& generic_functions,
    std::unordered_map<std::string, LoweredFunctionSignature> const& functions,
    std::unordered_map<std::string, std::string> const& local_source_types
) -> std::optional<std::string>;

auto bind_generic_function_call_substitutions(
    syntax::FunctionSyntax const& function,
    syntax::ExpressionSyntax const& call,
    std::unordered_map<std::string, syntax::FunctionSyntax const*> const& generic_functions,
    std::unordered_map<std::string, LoweredFunctionSignature> const& functions,
    std::unordered_map<std::string, std::string> const& local_source_types
) -> std::optional<std::unordered_map<std::string, syntax::TypeSyntax>> {
    if (function.generic_parameters.empty() || call.arguments.size() != function.parameters.size()) {
        return std::nullopt;
    }

    auto generic_parameters = generic_parameter_set(function.generic_parameters);
    auto substitutions = std::unordered_map<std::string, syntax::TypeSyntax> {};
    for (auto index = std::size_t {0}; index < call.arguments.size(); ++index) {
        auto const& argument = call.arguments[index];
        auto source_type_name = source_type_name_for_generic_specialization_argument(
            argument,
            generic_functions,
            functions,
            local_source_types
        );
        if (!source_type_name.has_value()) {
            return std::nullopt;
        }
        if (!unify_constructor_type(
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

auto source_type_name_for_generic_specialization_argument(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::FunctionSyntax const*> const& generic_functions,
    std::unordered_map<std::string, LoweredFunctionSignature> const& functions,
    std::unordered_map<std::string, std::string> const& local_source_types
) -> std::optional<std::string> {
    if (expression.kind == syntax::ExpressionKind::name) {
        auto source_type = local_source_types.find(expression.text);
        if (source_type != local_source_types.end()) {
            return source_type->second;
        }
        return std::nullopt;
    }

    if (expression.kind != syntax::ExpressionKind::call ||
        expression.left == nullptr ||
        expression.left->kind != syntax::ExpressionKind::name) {
        return std::nullopt;
    }

    auto generic_function = generic_functions.find(expression.left->text);
    if (generic_function != generic_functions.end()) {
        auto substitutions = bind_generic_function_call_substitutions(
            *generic_function->second,
            expression,
            generic_functions,
            functions,
            local_source_types
        );
        if (!substitutions.has_value()) {
            return std::nullopt;
        }
        return render_source_type_name(substitute_type(generic_function->second->return_type, *substitutions));
    }

    auto source_function = functions.find(expression.left->text);
    if (source_function != functions.end() && !source_function->second.source_return_type_name.empty()) {
        return source_function->second.source_return_type_name;
    }
    return std::nullopt;
}

void record_dynamic_array_descriptor_parameter_types(
    syntax::FunctionSyntax const& function,
    LoweredFunctionSignature& signature
) {
    for (auto index = std::size_t {0}; index < function.parameters.size(); ++index) {
        if (index >= signature.parameter_types.size()) {
            continue;
        }
        auto source_type_name = render_source_type_name(function.parameters[index].type);
        auto sequence = dynamic_sequence_source_type(source_type_name);
        if (!sequence.has_value() ||
            sequence->kind != DynamicSequenceKind::dynamic_array ||
            !is_scalar_or_nonowning_source_type(sequence->element_source_type_name)) {
            continue;
        }
        signature.parameter_types[index] = std::string {dynamic_array_descriptor_llvm_type()};
        signature.parameter_signedness[index] = IntegerSignedness::not_integer;
    }
}

auto specialized_function_copy(
    syntax::FunctionSyntax const& function,
    std::unordered_map<std::string, syntax::TypeSyntax> const& substitutions
) -> syntax::FunctionSyntax {
    auto specialized = clone_function(function);
    specialized.name = specialized_function_symbol_name(function, substitutions);
    specialized.generic_parameters.clear();
    specialized.return_type = substitute_type(specialized.return_type, substitutions);
    for (auto& parameter : specialized.parameters) {
        parameter.type = substitute_type(parameter.type, substitutions);
    }
    return specialized;
}

void collect_generic_calls_from_expression(
    syntax::ExpressionSyntax const& expression,
    std::unordered_map<std::string, syntax::FunctionSyntax const*> const& generic_functions,
    std::unordered_map<std::string, LoweredFunctionSignature> const& functions,
    std::unordered_map<std::string, std::string> const& local_source_types,
    std::vector<std::shared_ptr<syntax::FunctionSyntax>>& specializations
) {
    if (expression.kind == syntax::ExpressionKind::call &&
        expression.left != nullptr &&
        expression.left->kind == syntax::ExpressionKind::name) {
        auto function = generic_functions.find(expression.left->text);
        if (function != generic_functions.end()) {
            if (auto substitutions =
                    bind_generic_function_call_substitutions(
                        *function->second,
                        expression,
                        generic_functions,
                        functions,
                        local_source_types
                    )) {
                auto specialization = specialized_function_copy(*function->second, *substitutions);
                auto already_recorded = std::ranges::any_of(
                    specializations,
                    [&](std::shared_ptr<syntax::FunctionSyntax> const& existing) {
                        return existing->name == specialization.name;
                    }
                );
                if (!already_recorded) {
                    specializations.push_back(
                        std::make_shared<syntax::FunctionSyntax>(std::move(specialization))
                    );
                }
            }
        }
    }

    for (auto const& argument : expression.arguments) {
        collect_generic_calls_from_expression(argument, generic_functions, functions, local_source_types, specializations);
    }
    for (auto const& nested_statement : expression.nested_statements) {
        collect_generic_calls_from_statement(*nested_statement, generic_functions, functions, local_source_types, specializations);
    }
    if (expression.left != nullptr) {
        collect_generic_calls_from_expression(*expression.left, generic_functions, functions, local_source_types, specializations);
    }
    if (expression.right != nullptr) {
        collect_generic_calls_from_expression(*expression.right, generic_functions, functions, local_source_types, specializations);
    }
    if (expression.alternate != nullptr) {
        collect_generic_calls_from_expression(
            *expression.alternate,
            generic_functions,
            functions,
            local_source_types,
            specializations
        );
    }
}

void collect_generic_calls_from_statement(
    syntax::StatementSyntax const& statement,
    std::unordered_map<std::string, syntax::FunctionSyntax const*> const& generic_functions,
    std::unordered_map<std::string, LoweredFunctionSignature> const& functions,
    std::unordered_map<std::string, std::string> const& local_source_types,
    std::vector<std::shared_ptr<syntax::FunctionSyntax>>& specializations
) {
    collect_generic_calls_from_expression(statement.assignment_target, generic_functions, functions, local_source_types, specializations);
    collect_generic_calls_from_expression(statement.expression, generic_functions, functions, local_source_types, specializations);
    for (auto const& nested_statement : statement.nested_statements) {
        collect_generic_calls_from_statement(nested_statement, generic_functions, functions, local_source_types, specializations);
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        collect_generic_calls_from_statement(alternate_statement, generic_functions, functions, local_source_types, specializations);
    }
    for (auto const& switch_case : statement.switch_cases) {
        collect_generic_calls_from_expression(switch_case.pattern, generic_functions, functions, local_source_types, specializations);
        for (auto const& case_statement : switch_case.statements) {
            collect_generic_calls_from_statement(*case_statement, generic_functions, functions, local_source_types, specializations);
        }
    }
}

auto collect_generic_function_specializations(
    syntax::ModuleSyntax const& module,
    std::unordered_map<std::string, LoweredFunctionSignature> const& functions
) -> std::vector<std::shared_ptr<syntax::FunctionSyntax>> {
    auto generic_functions = std::unordered_map<std::string, syntax::FunctionSyntax const*> {};
    for (auto const& function : module.functions) {
        if (!function.generic_parameters.empty()) {
            generic_functions.emplace(function.name, &function);
        }
    }
    if (generic_functions.empty()) {
        return {};
    }

    auto specializations = std::vector<std::shared_ptr<syntax::FunctionSyntax>> {};
    for (auto const& function : module.functions) {
        auto local_source_types = std::unordered_map<std::string, std::string> {};
        for (auto const& parameter : function.parameters) {
            local_source_types[parameter.name] = render_source_type_name(parameter.type);
        }
        for (auto const& statement : function.body_statements) {
            if ((statement.kind == syntax::StatementKind::let_binding ||
                 statement.kind == syntax::StatementKind::var_binding) &&
                !statement.annotated_type.name.empty()) {
                local_source_types[statement.name] = render_source_type_name(statement.annotated_type);
            } else if ((statement.kind == syntax::StatementKind::let_binding ||
                        statement.kind == syntax::StatementKind::var_binding) &&
                       !statement.name.empty()) {
                auto inferred_source_type = source_type_name_for_generic_specialization_argument(
                    statement.expression,
                    generic_functions,
                    functions,
                    local_source_types
                );
                if (inferred_source_type.has_value()) {
                    local_source_types[statement.name] = std::move(*inferred_source_type);
                }
            }
            collect_generic_calls_from_statement(statement, generic_functions, functions, local_source_types, specializations);
        }
    }
    return specializations;
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
    std::unordered_map<std::string, syntax::ChoiceSyntax const*> const& generic_choices,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
);

void collect_statement_type_instantiations(
    syntax::StatementSyntax const& statement,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::unordered_map<std::string, syntax::ChoiceSyntax const*> const& generic_choices,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
);

void collect_type_instantiations(
    syntax::TypeSyntax const& type,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::unordered_map<std::string, syntax::ChoiceSyntax const*> const& generic_choices,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
) {
    for (auto const& argument : type.generic_arguments) {
        collect_type_instantiations(argument, generic_records, generic_choices, pending, seen);
    }

    auto record = generic_records.find(type.name);
    auto choice = generic_choices.find(type.name);
    auto const expected_argument_count = record != generic_records.end()
        ? record->second->generic_parameters.size()
        : choice != generic_choices.end()
            ? choice->second->generic_parameters.size()
            : std::size_t {0};
    if (record == generic_records.end() && choice == generic_choices.end()) {
        return;
    }
    if (type.generic_arguments.size() != expected_argument_count) {
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
    std::unordered_map<std::string, syntax::ChoiceSyntax const*> const& generic_choices,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
) {
    if (auto inferred_type = infer_constructor_expression_type(expression, generic_records)) {
        collect_type_instantiations(*inferred_type, generic_records, generic_choices, pending, seen);
    }

    for (auto const& argument : expression.arguments) {
        collect_expression_type_instantiations(argument, generic_records, generic_choices, pending, seen);
    }
    if (expression.left != nullptr) {
        collect_expression_type_instantiations(*expression.left, generic_records, generic_choices, pending, seen);
    }
    if (expression.right != nullptr) {
        collect_expression_type_instantiations(*expression.right, generic_records, generic_choices, pending, seen);
    }
    if (expression.alternate != nullptr) {
        collect_expression_type_instantiations(*expression.alternate, generic_records, generic_choices, pending, seen);
    }
    for (auto const& statement : expression.nested_statements) {
        if (statement != nullptr) {
            collect_statement_type_instantiations(*statement, generic_records, generic_choices, pending, seen);
        }
    }
}

void collect_statement_type_instantiations(
    syntax::StatementSyntax const& statement,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::unordered_map<std::string, syntax::ChoiceSyntax const*> const& generic_choices,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
) {
    if (!statement.annotated_type.name.empty()) {
        collect_type_instantiations(statement.annotated_type, generic_records, generic_choices, pending, seen);
    }
    collect_expression_type_instantiations(statement.assignment_target, generic_records, generic_choices, pending, seen);
    collect_expression_type_instantiations(statement.expression, generic_records, generic_choices, pending, seen);
    for (auto const& nested_statement : statement.nested_statements) {
        collect_statement_type_instantiations(nested_statement, generic_records, generic_choices, pending, seen);
    }
    for (auto const& alternate_statement : statement.alternate_statements) {
        collect_statement_type_instantiations(alternate_statement, generic_records, generic_choices, pending, seen);
    }
    for (auto const& switch_case : statement.switch_cases) {
        collect_expression_type_instantiations(switch_case.pattern, generic_records, generic_choices, pending, seen);
        for (auto const& case_statement : switch_case.statements) {
            if (case_statement != nullptr) {
                collect_statement_type_instantiations(*case_statement, generic_records, generic_choices, pending, seen);
            }
        }
    }
}

void collect_function_type_instantiations(
    syntax::FunctionSyntax const& function,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::unordered_map<std::string, syntax::ChoiceSyntax const*> const& generic_choices,
    std::vector<syntax::TypeSyntax>& pending,
    std::unordered_set<std::string>& seen
) {
    collect_type_instantiations(function.return_type, generic_records, generic_choices, pending, seen);
    for (auto const& parameter : function.parameters) {
        collect_type_instantiations(parameter.type, generic_records, generic_choices, pending, seen);
    }
    for (auto const& constraint : function.where_constraints) {
        for (auto const& requirement : constraint.requirements) {
            collect_type_instantiations(requirement, generic_records, generic_choices, pending, seen);
        }
    }
    for (auto const& statement : function.body_statements) {
        collect_statement_type_instantiations(statement, generic_records, generic_choices, pending, seen);
    }
}

auto collect_generic_type_instantiations(
    syntax::ModuleSyntax const& module,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::unordered_map<std::string, syntax::ChoiceSyntax const*> const& generic_choices
) -> std::vector<syntax::TypeSyntax> {
    auto pending = std::vector<syntax::TypeSyntax> {};
    auto seen = std::unordered_set<std::string> {};

    for (auto const& alias : module.type_aliases) {
        collect_type_instantiations(alias.aliased_type, generic_records, generic_choices, pending, seen);
    }
    for (auto const& constant : module.constants) {
        collect_type_instantiations(constant.type, generic_records, generic_choices, pending, seen);
    }
    for (auto const& record : module.records) {
        for (auto const& field : record.fields) {
            collect_type_instantiations(field.type, generic_records, generic_choices, pending, seen);
        }
    }
    for (auto const& choice : module.choices) {
        for (auto const& variant : choice.variants) {
            for (auto const& payload : variant.payloads) {
                collect_type_instantiations(payload.type, generic_records, generic_choices, pending, seen);
            }
        }
    }
    for (auto const& interface : module.interfaces) {
        for (auto const& method : interface.methods) {
            collect_type_instantiations(method.return_type, generic_records, generic_choices, pending, seen);
            for (auto const& parameter : method.parameters) {
                collect_type_instantiations(parameter.type, generic_records, generic_choices, pending, seen);
            }
            for (auto const& constraint : method.where_constraints) {
                for (auto const& requirement : constraint.requirements) {
                    collect_type_instantiations(requirement, generic_records, generic_choices, pending, seen);
                }
            }
        }
    }
    for (auto const& foreign_import : module.foreign_imports) {
        for (auto const& function : foreign_import.functions) {
            collect_type_instantiations(function.return_type, generic_records, generic_choices, pending, seen);
            for (auto const& parameter : function.parameters) {
                collect_type_instantiations(parameter.type, generic_records, generic_choices, pending, seen);
            }
        }
    }
    for (auto const& foreign_export : module.foreign_exports) {
        collect_function_type_instantiations(foreign_export.function, generic_records, generic_choices, pending, seen);
    }
    for (auto const& implementation : module.implementations) {
        collect_type_instantiations(implementation.interface_type, generic_records, generic_choices, pending, seen);
        collect_type_instantiations(implementation.receiver_type, generic_records, generic_choices, pending, seen);
        for (auto const& method : implementation.methods) {
            collect_function_type_instantiations(method, generic_records, generic_choices, pending, seen);
        }
    }
    for (auto const& extension : module.extensions) {
        collect_type_instantiations(extension.receiver_type, generic_records, generic_choices, pending, seen);
        for (auto const& method : extension.methods) {
            collect_function_type_instantiations(method, generic_records, generic_choices, pending, seen);
        }
    }
    for (auto const& function : module.functions) {
        collect_function_type_instantiations(function, generic_records, generic_choices, pending, seen);
    }

    for (auto index = std::size_t {0}; index < pending.size(); ++index) {
        auto record = generic_records.find(pending[index].name);
        auto choice = generic_choices.find(pending[index].name);
        if (record != generic_records.end()) {
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
                    generic_choices,
                    pending,
                    seen
                );
            }
        }
        if (choice != generic_choices.end()) {
            auto substitutions = std::unordered_map<std::string, syntax::TypeSyntax> {};
            for (auto argument_index = std::size_t {0}; argument_index < pending[index].generic_arguments.size();
                 ++argument_index) {
                substitutions.emplace(
                    choice->second->generic_parameters[argument_index],
                    pending[index].generic_arguments[argument_index]
                );
            }
            for (auto const& variant : choice->second->variants) {
                for (auto const& payload : variant.payloads) {
                    collect_type_instantiations(
                        substitute_type(payload.type, substitutions),
                        generic_records,
                        generic_choices,
                        pending,
                        seen
                    );
                }
            }
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
    auto choice = choices.find(render_source_type_name(type));
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
    if (choices != nullptr) {
        auto choice = choices->find(render_source_type_name(type));
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
    return !type.empty() && type != "void";
}

auto choice_payload_storage_type(std::size_t size_bytes) -> std::string {
    return "[" + std::to_string(std::max<std::size_t>(size_bytes, 1)) + " x i8]";
}

auto choice_variant_payload_type(std::vector<LoweredChoicePayload> const& payloads) -> std::string {
    if (payloads.empty()) {
        return {};
    }
    if (payloads.size() == 1) {
        return payloads.front().llvm_type;
    }

    auto payload_type = std::string {"{ "};
    for (auto index = std::size_t {0}; index < payloads.size(); ++index) {
        if (index > 0) {
            payload_type += ", ";
        }
        payload_type += payloads[index].llvm_type;
    }
    payload_type += " }";
    return payload_type;
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
    std::unordered_set<std::string> const& record_names,
    std::unordered_map<std::string, LoweredChoiceLayout> const& choices,
    syntax::TypeSyntax concrete_type = {},
    LoweringContext const* sizing_context = nullptr
) -> LoweredChoiceLayout {
    auto const is_template_layout = concrete_type.name.empty();
    auto choice_type = concrete_type.name.empty()
        ? syntax::TypeSyntax {.name = choice.name}
        : std::move(concrete_type);
    if (choice_type.generic_arguments.empty()) {
        for (auto const& generic_parameter : choice.generic_parameters) {
            choice_type.generic_arguments.push_back(syntax::TypeSyntax {
                .name = generic_parameter,
            });
        }
    }
    auto substitutions = std::unordered_map<std::string, syntax::TypeSyntax> {};
    if (choice_type.generic_arguments.size() == choice.generic_parameters.size()) {
        for (auto index = std::size_t {0}; index < choice.generic_parameters.size(); ++index) {
            substitutions.emplace(choice.generic_parameters[index], choice_type.generic_arguments[index]);
        }
    }

    auto layout = LoweredChoiceLayout {
        .name = is_template_layout ? choice.name : render_source_type_name(choice_type),
        .source_type_name = render_source_type_name(choice_type),
        .generic_parameters = choice.generic_parameters,
    };
    layout.variants.reserve(choice.variants.size());
    auto payload_llvm_type = std::optional<std::string> {};
    auto max_payload_size_bytes = std::size_t {0};
    auto has_distinct_payload_types = false;
    auto missing_distinct_payload_size = std::optional<std::string> {};
    auto has_payload = false;
    auto const is_concrete_instantiation = choice.generic_parameters.empty() ||
        generic_parameter_set(choice).empty() ||
        choice_type.generic_arguments.size() == choice.generic_parameters.size();
    auto supports_payload_abi = choice.generic_parameters.empty() || is_concrete_instantiation;
    if (!supports_payload_abi) {
        layout.unsupported_abi_reason = "generic choices do not yet have a lowered choice ABI";
    }
    for (auto variant_index = std::size_t {0}; variant_index < choice.variants.size(); ++variant_index) {
        auto const& variant = choice.variants[variant_index];
        auto lowered_variant = LoweredChoiceVariant {
            .name = variant.name,
            .tag = variant_index,
        };
        lowered_variant.payloads.reserve(variant.payloads.size());
        for (auto payload_index = std::size_t {0}; payload_index < variant.payloads.size(); ++payload_index) {
            has_payload = true;
            auto const& payload = variant.payloads[payload_index];
            auto substituted_payload_type = substitute_type(payload.type, substitutions);
            auto payload_llvm = llvm_field_type_for(substituted_payload_type, record_names, &choices);
            if (!is_supported_choice_payload_llvm_type(payload_llvm)) {
                supports_payload_abi = false;
                if (layout.unsupported_abi_reason.empty()) {
                    layout.unsupported_abi_reason =
                        "choice payload type '" + render_source_type_name(substituted_payload_type) +
                        "' does not yet have a lowered choice ABI";
                }
            }
            lowered_variant.payloads.push_back(LoweredChoicePayload {
                .name = payload.name,
                .source_type_name = render_source_type_name(substituted_payload_type),
                .llvm_type = std::move(payload_llvm),
                .index = payload_index,
            });
        }
        lowered_variant.lowered_payload_type = choice_variant_payload_type(lowered_variant.payloads);
        if (!lowered_variant.lowered_payload_type.empty()) {
            if (!payload_llvm_type.has_value()) {
                payload_llvm_type = lowered_variant.lowered_payload_type;
            } else if (*payload_llvm_type != lowered_variant.lowered_payload_type) {
                has_distinct_payload_types = true;
            }

            auto payload_size = sizing_context == nullptr
                ? lowered_type_size_bytes(lowered_variant.lowered_payload_type)
                : lowered_type_size_bytes(lowered_variant.lowered_payload_type, *sizing_context);
            if (!payload_size.has_value()) {
                payload_size = lowered_type_size_bytes(lowered_variant.lowered_payload_type);
            }
            if (!payload_size.has_value()) {
                missing_distinct_payload_size = lowered_variant.name;
            } else {
                max_payload_size_bytes = std::max(max_payload_size_bytes, *payload_size);
            }
        }
        layout.variants.push_back(std::move(lowered_variant));
    }
    if (supports_payload_abi && has_distinct_payload_types && missing_distinct_payload_size.has_value()) {
        supports_payload_abi = false;
        layout.unsupported_abi_reason =
            "choice variant '" + *missing_distinct_payload_size +
            "' does not yet have a finite lowered choice ABI size";
    }
    if (supports_payload_abi) {
        layout.llvm_type_name = has_payload
            ? "{ i32, " +
                (has_distinct_payload_types
                    ? choice_payload_storage_type(max_payload_size_bytes)
                    : *payload_llvm_type) +
                " }"
            : "i32";
        layout.unsupported_abi_reason.clear();
    }
    return layout;
}

void collect_all_choice_layouts(
    syntax::ModuleSyntax const& module,
    std::vector<syntax::TypeSyntax> const& instantiated_types,
    std::unordered_map<std::string, syntax::ChoiceSyntax const*> const& generic_choices,
    std::unordered_set<std::string> const& record_names,
    LoweringContext& context,
    LoweringContext const* sizing_context = nullptr
) {
    context.choices.clear();
    for (auto const& choice : module.choices) {
        context.choices.emplace(
            choice.name,
            collect_choice_layout(choice, record_names, context.choices, {}, sizing_context)
        );
    }
    for (auto const& type : instantiated_types) {
        auto choice = generic_choices.find(type.name);
        if (choice == generic_choices.end()) {
            continue;
        }
        auto source_type_name = render_source_type_name(type);
        context.choices.emplace(
            source_type_name,
            collect_choice_layout(*choice->second, record_names, context.choices, type, sizing_context)
        );
    }
}

void collect_all_record_layouts(
    syntax::ModuleSyntax const& module,
    std::vector<syntax::TypeSyntax> const& instantiated_types,
    std::unordered_map<std::string, syntax::RecordSyntax const*> const& generic_records,
    std::unordered_set<std::string> const& record_names,
    LoweringContext& context
) {
    context.records.clear();
    for (auto const& record : module.records) {
        if (!record.generic_parameters.empty()) {
            continue;
        }
        context.records.emplace(record.name, collect_record_layout(record, record_names, context.choices));
    }
    for (auto const& type : instantiated_types) {
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
}

}  // namespace

auto build_lowering_context(
    syntax::ModuleSyntax const& module,
    diagnostics::DiagnosticBag& diagnostics
) -> LoweringContext {
    auto context = LoweringContext {};
    auto record_names = std::unordered_set<std::string> {};
    auto generic_records = std::unordered_map<std::string, syntax::RecordSyntax const*> {};
    auto generic_choices = std::unordered_map<std::string, syntax::ChoiceSyntax const*> {};
    for (auto const& record : module.records) {
        if (!record.generic_parameters.empty()) {
            generic_records.emplace(record.name, &record);
            context.generic_record_parameters.emplace(record.name, record.generic_parameters);
            continue;
        }
        record_names.insert(record.name);
    }
    for (auto const& choice : module.choices) {
        if (!choice.generic_parameters.empty() && choice.name != "Maybe") {
            generic_choices.emplace(choice.name, &choice);
        }
    }
    auto instantiated_types = collect_generic_type_instantiations(module, generic_records, generic_choices);
    for (auto const& type : instantiated_types) {
        if (generic_records.contains(type.name)) {
            record_names.insert(render_record_type_name(type));
        }
    }
    collect_all_choice_layouts(module, instantiated_types, generic_choices, record_names, context);
    collect_all_record_layouts(module, instantiated_types, generic_records, record_names, context);
    collect_all_choice_layouts(module, instantiated_types, generic_choices, record_names, context, &context);
    collect_all_record_layouts(module, instantiated_types, generic_records, record_names, context);

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

    context.generic_function_specializations = collect_generic_function_specializations(module, context.functions);
    auto specialization_counts = std::unordered_map<std::string, std::size_t> {};
    for (auto const& specialization_ptr : context.generic_function_specializations) {
        auto delimiter = specialization_ptr->name.find("__");
        if (delimiter != std::string::npos) {
            ++specialization_counts[specialization_ptr->name.substr(0, delimiter)];
        }
    }
    for (auto const& specialization_ptr : context.generic_function_specializations) {
        auto const& specialization = *specialization_ptr;
        auto signature = lower_contextual_function_signature(
            specialization.return_type,
            specialization.parameters,
            specialization.name,
            record_names,
            context.choices
        );
        record_dynamic_array_descriptor_parameter_types(specialization, signature);
        if (!has_supported_function_signature_types(signature)) {
            continue;
        }
        auto original_name = std::string {};
        auto delimiter = specialization.name.find("__");
        if (delimiter != std::string::npos) {
            original_name = specialization.name.substr(0, delimiter);
        }
        context.functions[specialization.name] = signature;
        if (!original_name.empty() && specialization_counts[original_name] == 1) {
            context.functions[original_name] = signature;
        }
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
