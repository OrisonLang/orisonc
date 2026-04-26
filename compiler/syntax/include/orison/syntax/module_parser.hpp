#pragma once

#include "orison/diagnostics/diagnostic_bag.hpp"
#include "orison/source/source_file.hpp"

#include <memory>
#include <string>
#include <vector>

namespace orison::syntax {

enum class Visibility {
    public_visibility,
    package_visibility,
    private_visibility,
};

struct TypeSyntax {
    std::string name;
    std::vector<TypeSyntax> generic_arguments;
};

struct ImportSyntax {
    std::string name;
    std::string alias;
    std::string from_package;
};

struct TypeAliasSyntax {
    Visibility visibility = Visibility::package_visibility;
    std::string name;
    TypeSyntax aliased_type;
};

struct NamedTypeSyntax {
    std::string name;
    TypeSyntax type;
};

struct FieldSyntax {
    Visibility visibility = Visibility::private_visibility;
    std::string name;
    TypeSyntax type;
};

struct ParameterSyntax {
    std::string name;
    TypeSyntax type;
};

struct WhereConstraintSyntax {
    std::string parameter_name;
    std::vector<TypeSyntax> requirements;
};

enum class ExpressionKind {
    name,
    integer_literal,
    unary,
    call,
    member_access,
    binary,
};

struct ExpressionSyntax {
    ExpressionKind kind = ExpressionKind::name;
    std::string text;
    std::vector<ExpressionSyntax> arguments;
    std::unique_ptr<ExpressionSyntax> left;
    std::unique_ptr<ExpressionSyntax> right;
};

struct StatementSyntax;

struct SwitchCaseSyntax {
    bool is_default = false;
    ExpressionSyntax pattern;
    std::vector<std::unique_ptr<StatementSyntax>> statements;
};

enum class StatementKind {
    let_binding,
    var_binding,
    assignment_statement,
    return_statement,
    break_statement,
    continue_statement,
    switch_statement,
    guard_statement,
    if_statement,
    while_statement,
    for_statement,
    defer_statement,
    expression_statement,
};

struct StatementSyntax {
    StatementKind kind = StatementKind::expression_statement;
    bool valid = true;
    std::string name;
    TypeSyntax annotated_type;
    ExpressionSyntax assignment_target;
    ExpressionSyntax expression;
    std::vector<StatementSyntax> nested_statements;
    std::vector<StatementSyntax> alternate_statements;
    std::vector<SwitchCaseSyntax> switch_cases;
};

struct RecordSyntax {
    Visibility visibility = Visibility::package_visibility;
    std::string name;
    std::vector<std::string> generic_parameters;
    std::vector<FieldSyntax> fields;
};

struct ChoiceVariantSyntax {
    std::string name;
    std::vector<NamedTypeSyntax> payloads;
};

struct ChoiceSyntax {
    Visibility visibility = Visibility::package_visibility;
    std::string name;
    std::vector<std::string> generic_parameters;
    std::vector<ChoiceVariantSyntax> variants;
};

struct InterfaceMethodSyntax {
    std::string name;
    std::vector<std::string> generic_parameters;
    std::vector<ParameterSyntax> parameters;
    TypeSyntax return_type;
    std::vector<WhereConstraintSyntax> where_constraints;
};

struct InterfaceSyntax {
    Visibility visibility = Visibility::package_visibility;
    std::string name;
    std::vector<std::string> generic_parameters;
    std::vector<InterfaceMethodSyntax> methods;
};

struct FunctionSyntax {
    Visibility visibility = Visibility::package_visibility;
    std::string name;
    std::vector<std::string> generic_parameters;
    std::vector<ParameterSyntax> parameters;
    TypeSyntax return_type;
    std::vector<WhereConstraintSyntax> where_constraints;
    std::vector<StatementSyntax> body_statements;
};

struct ImplementationSyntax {
    TypeSyntax interface_type;
    TypeSyntax receiver_type;
    std::vector<FunctionSyntax> methods;
};

struct ExtensionSyntax {
    TypeSyntax receiver_type;
    std::vector<FunctionSyntax> methods;
};

struct ModuleSyntax {
    std::string package_name;
    std::vector<ImportSyntax> imports;
    std::vector<TypeAliasSyntax> type_aliases;
    std::vector<RecordSyntax> records;
    std::vector<ChoiceSyntax> choices;
    std::vector<InterfaceSyntax> interfaces;
    std::vector<ImplementationSyntax> implementations;
    std::vector<ExtensionSyntax> extensions;
    std::vector<FunctionSyntax> functions;
    std::size_t top_level_declaration_count = 0;
};

struct ParseResult {
    ModuleSyntax module;
    diagnostics::DiagnosticBag diagnostics;
};

class ModuleParser {
public:
    auto parse(source::SourceFile const& source_file) const -> ParseResult;
};

}  // namespace orison::syntax
