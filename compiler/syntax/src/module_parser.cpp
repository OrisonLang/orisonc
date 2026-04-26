#include "orison/syntax/module_parser.hpp"

#include "orison/syntax/lexer.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace orison::syntax {
namespace {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    auto parse() -> ParseResult {
        ParseResult result;

        while (!is(TokenKind::eof)) {
            skip_layout_between_declarations();
            if (is(TokenKind::eof)) {
                break;
            }

            auto const& token = current();
            if (!token.line_start || token.indent != 0) {
                skip_to_next_top_level();
                continue;
            }

            parse_top_level_declaration(result);
        }

        if (result.module.package_name.empty()) {
            result.diagnostics.error(1, "source file must start with a package declaration");
        }

        return result;
    }

private:
    void parse_top_level_declaration(ParseResult& result) {
        if (current_starts_visibility_qualified_declaration()) {
            auto visibility = visibility_from_token(current().kind);
            advance();
            parse_visibility_qualified_top_level_declaration(result, visibility);
            return;
        }

        switch (current().kind) {
        case TokenKind::keyword_package:
            parse_package(result);
            break;
        case TokenKind::keyword_import:
            parse_import(result);
            break;
        case TokenKind::keyword_type:
            parse_type_alias(result, Visibility::package_visibility);
            break;
        case TokenKind::keyword_record:
            parse_record(result, Visibility::package_visibility);
            break;
        case TokenKind::keyword_choice:
            parse_choice(result, Visibility::package_visibility);
            break;
        case TokenKind::keyword_interface:
            parse_interface(result, Visibility::package_visibility);
            break;
        case TokenKind::keyword_implements:
            parse_implementation(result);
            break;
        case TokenKind::keyword_extend:
            parse_extension(result);
            break;
        case TokenKind::keyword_function:
            parse_function(result, Visibility::package_visibility);
            break;
        default:
            result.diagnostics.error(
                current().line,
                "expected a top-level declaration such as package, import, type, record, choice, interface, implements, extend, or function"
            );
            skip_to_next_line();
            break;
        }
    }

    void parse_visibility_qualified_top_level_declaration(ParseResult& result, Visibility visibility) {
        switch (current().kind) {
        case TokenKind::keyword_type:
            parse_type_alias(result, visibility);
            break;
        case TokenKind::keyword_record:
            parse_record(result, visibility);
            break;
        case TokenKind::keyword_choice:
            parse_choice(result, visibility);
            break;
        case TokenKind::keyword_interface:
            parse_interface(result, visibility);
            break;
        case TokenKind::keyword_function:
            parse_function(result, visibility);
            break;
        default:
            result.diagnostics.error(current().line, "visibility modifier must be followed by type, record, choice, interface, or function");
            skip_to_next_line();
            break;
        }
    }

    auto current() const -> Token const& {
        return tokens_[index_];
    }

    auto peek(std::size_t offset = 1) const -> Token const& {
        auto target = index_ + offset;
        if (target >= tokens_.size()) {
            return tokens_.back();
        }
        return tokens_[target];
    }

    auto is(TokenKind kind) const -> bool {
        return current().kind == kind;
    }

    auto is_visibility_modifier(TokenKind kind) const -> bool {
        return kind == TokenKind::keyword_public || kind == TokenKind::keyword_package ||
               kind == TokenKind::keyword_private;
    }

    auto visibility_from_token(TokenKind kind) const -> Visibility {
        switch (kind) {
        case TokenKind::keyword_public:
            return Visibility::public_visibility;
        case TokenKind::keyword_private:
            return Visibility::private_visibility;
        case TokenKind::keyword_package:
            return Visibility::package_visibility;
        default:
            return Visibility::package_visibility;
        }
    }

    auto current_starts_visibility_qualified_declaration() const -> bool {
        if (!is_visibility_modifier(current().kind)) {
            return false;
        }

        auto const& next = peek();
        return next.kind == TokenKind::keyword_record || next.kind == TokenKind::keyword_choice ||
               next.kind == TokenKind::keyword_interface || next.kind == TokenKind::keyword_function ||
               next.kind == TokenKind::keyword_type;
    }

    void advance() {
        if (index_ < tokens_.size() - 1) {
            ++index_;
        }
    }

    void skip_newlines() {
        while (is(TokenKind::newline)) {
            advance();
        }
    }

    void skip_layout_between_declarations() {
        while (is(TokenKind::newline) || is(TokenKind::dedent)) {
            advance();
        }
    }

    void skip_to_next_line() {
        while (!is(TokenKind::newline) && !is(TokenKind::eof)) {
            advance();
        }
        skip_newlines();
    }

    void skip_to_next_top_level() {
        while (!is(TokenKind::eof)) {
            if (is(TokenKind::newline) || is(TokenKind::dedent)) {
                advance();
                if (current().line_start) {
                    break;
                }
                continue;
            }
            advance();
        }
    }

    auto expect_identifier(ParseResult& result, std::string const& message) -> std::string {
        if (!is(TokenKind::identifier)) {
            result.diagnostics.error(current().line, message);
            return {};
        }

        auto value = current().lexeme;
        advance();
        return value;
    }

    auto is_name_component_token(TokenKind kind) const -> bool {
        switch (kind) {
        case TokenKind::identifier:
        case TokenKind::keyword_package:
        case TokenKind::keyword_import:
        case TokenKind::keyword_type:
        case TokenKind::keyword_record:
        case TokenKind::keyword_choice:
        case TokenKind::keyword_interface:
        case TokenKind::keyword_function:
        case TokenKind::keyword_public:
        case TokenKind::keyword_private:
        case TokenKind::keyword_implements:
        case TokenKind::keyword_extend:
        case TokenKind::keyword_from:
        case TokenKind::keyword_as:
        case TokenKind::keyword_shared:
        case TokenKind::keyword_exclusive:
        case TokenKind::keyword_let:
        case TokenKind::keyword_var:
        case TokenKind::keyword_return:
        case TokenKind::keyword_switch:
        case TokenKind::keyword_default:
        case TokenKind::keyword_guard:
        case TokenKind::keyword_if:
        case TokenKind::keyword_else:
        case TokenKind::keyword_while:
        case TokenKind::keyword_for:
        case TokenKind::keyword_in:
        case TokenKind::keyword_defer:
        case TokenKind::keyword_break:
        case TokenKind::keyword_continue:
        case TokenKind::keyword_repeat:
        case TokenKind::keyword_unsafe:
        case TokenKind::keyword_where:
        case TokenKind::keyword_and:
        case TokenKind::keyword_or:
        case TokenKind::keyword_not:
        case TokenKind::keyword_bit_and:
        case TokenKind::keyword_bit_or:
        case TokenKind::keyword_bit_xor:
        case TokenKind::keyword_bit_not:
        case TokenKind::keyword_shift_left:
        case TokenKind::keyword_shift_right:
        case TokenKind::keyword_true:
        case TokenKind::keyword_false:
            return true;
        default:
            return false;
        }
    }

    auto expect_name_component(ParseResult& result, std::string const& message) -> std::string {
        if (!is_name_component_token(current().kind)) {
            result.diagnostics.error(current().line, message);
            return {};
        }

        auto value = current().lexeme;
        advance();
        return value;
    }

    auto parse_qualified_name(ParseResult& result, std::string const& message) -> std::string {
        auto name = expect_name_component(result, message);
        while (is(TokenKind::dot)) {
            advance();
            auto part = expect_name_component(result, "expected identifier after '.' in qualified name");
            if (part.empty()) {
                break;
            }
            name += ".";
            name += part;
        }
        return name;
    }

    auto parse_type(ParseResult& result, std::string const& message) -> TypeSyntax {
        std::string qualifier;
        if (is(TokenKind::keyword_shared) || is(TokenKind::keyword_exclusive)) {
            qualifier = current().lexeme;
            advance();
            if (is(TokenKind::dot)) {
                advance();
            }
        }

        TypeSyntax type {.name = parse_qualified_name(result, message)};
        if (type.name.empty()) {
            return type;
        }

        if (!qualifier.empty()) {
            type.name = qualifier + "." + type.name;
        }

        if (!is(TokenKind::less)) {
            return type;
        }

        advance();
        while (!is(TokenKind::greater) && !is(TokenKind::eof) && !is(TokenKind::newline)) {
            type.generic_arguments.push_back(parse_type(result, "expected generic type argument"));
            if (type.generic_arguments.back().name.empty()) {
                break;
            }

            if (is(TokenKind::comma)) {
                advance();
                continue;
            }

            if (!is(TokenKind::greater)) {
                result.diagnostics.error(current().line, "expected ',' or '>' in generic type");
                break;
            }
        }

        if (is(TokenKind::greater)) {
            advance();
        } else {
            result.diagnostics.error(current().line, "expected '>' to close generic type");
        }

        return type;
    }

    auto consume_block_start(ParseResult& result, std::string const& message) -> bool {
        if (!is(TokenKind::newline)) {
            result.diagnostics.error(current().line, message);
            return false;
        }

        advance();
        if (!is(TokenKind::indent)) {
            result.diagnostics.error(current().line, message);
            return false;
        }

        advance();
        skip_newlines();
        return true;
    }

    void consume_block_end() {
        if (is(TokenKind::dedent)) {
            advance();
        }
    }

    auto parse_parameter(ParseResult& result) -> ParameterSyntax {
        ParameterSyntax parameter;
        parameter.name = expect_identifier(result, "function parameter requires a name");
        if (parameter.name.empty()) {
            return parameter;
        }

        if (!is(TokenKind::colon)) {
            result.diagnostics.error(current().line, "function parameter requires ':' after the parameter name");
            parameter.name.clear();
            return parameter;
        }

        advance();
        parameter.type = parse_type(result, "function parameter requires a type");
        if (parameter.type.name.empty()) {
            parameter.name.clear();
        }
        return parameter;
    }

    auto parse_named_type(ParseResult& result, std::string const& name_message, std::string const& type_message)
        -> NamedTypeSyntax {
        NamedTypeSyntax named_type;
        named_type.name = expect_identifier(result, name_message);
        if (named_type.name.empty()) {
            return named_type;
        }

        if (!is(TokenKind::colon)) {
            result.diagnostics.error(current().line, "named type entry requires ':' after the name");
            named_type.name.clear();
            return named_type;
        }

        advance();
        named_type.type = parse_type(result, type_message);
        if (named_type.type.name.empty()) {
            named_type.name.clear();
        }
        return named_type;
    }

    auto parse_generic_parameter_names(ParseResult& result) -> std::vector<std::string> {
        std::vector<std::string> parameters;
        if (!is(TokenKind::less)) {
            return parameters;
        }

        advance();
        while (!is(TokenKind::eof)) {
            auto parameter = expect_identifier(result, "generic parameter list requires a parameter name");
            if (parameter.empty()) {
                break;
            }
            parameters.push_back(std::move(parameter));

            if (is(TokenKind::comma)) {
                advance();
                continue;
            }

            if (is(TokenKind::greater)) {
                advance();
                return parameters;
            }

            result.diagnostics.error(current().line, "expected ',' or '>' in generic parameter list");
            break;
        }

        if (is(TokenKind::greater)) {
            advance();
        } else {
            result.diagnostics.error(current().line, "generic parameter list cannot end before '>'");
        }

        return parameters;
    }

    auto parse_parameter_list(ParseResult& result) -> std::vector<ParameterSyntax> {
        std::vector<ParameterSyntax> parameters;
        if (!is(TokenKind::left_paren)) {
            result.diagnostics.error(current().line, "expected '(' after function name");
            return parameters;
        }

        advance();
        if (is(TokenKind::right_paren)) {
            advance();
            return parameters;
        }

        while (!is(TokenKind::eof)) {
            auto parameter = parse_parameter(result);
            if (!parameter.name.empty()) {
                parameters.push_back(std::move(parameter));
            } else {
                break;
            }

            if (is(TokenKind::comma)) {
                advance();
                continue;
            }

            if (is(TokenKind::right_paren)) {
                advance();
                return parameters;
            }

            result.diagnostics.error(current().line, "expected ',' or ')' in function parameter list");
            break;
        }

        if (!is(TokenKind::right_paren)) {
            result.diagnostics.error(current().line, "function header cannot end before ')'");
        } else {
            advance();
        }
        return parameters;
    }

    auto parse_function_signature(ParseResult& result, std::string const& declaration_message)
        -> std::optional<InterfaceMethodSyntax> {
        if (!is(TokenKind::keyword_function)) {
            result.diagnostics.error(current().line, declaration_message);
            return std::nullopt;
        }

        advance();
        auto name = expect_identifier(result, "function declaration requires a name");
        if (name.empty()) {
            return std::nullopt;
        }

        auto generic_parameters = parse_generic_parameter_names(result);
        auto parameters = parse_parameter_list(result);
        if (result.diagnostics.has_errors() && !is(TokenKind::arrow) && !is(TokenKind::newline)) {
            skip_to_next_line();
        }

        if (!is(TokenKind::arrow)) {
            result.diagnostics.error(current().line, "expected '->' after function parameter list");
            return std::nullopt;
        }

        advance();
        auto return_type = parse_type(result, "expected return type after '->'");
        if (return_type.name.empty()) {
            return std::nullopt;
        }

        std::vector<WhereConstraintSyntax> where_constraints;
        if (is(TokenKind::newline) && peek().kind == TokenKind::keyword_where && peek().line_start) {
            advance();
            advance();

            while (!is(TokenKind::newline) && !is(TokenKind::eof)) {
                WhereConstraintSyntax constraint;
                constraint.parameter_name =
                    expect_identifier(result, "where clause requires a constrained generic parameter name");
                if (constraint.parameter_name.empty()) {
                    break;
                }

                if (!is(TokenKind::colon)) {
                    result.diagnostics.error(current().line, "where clause requires ':' after the constrained parameter");
                    break;
                }

                advance();
                while (true) {
                    auto requirement = parse_type(result, "where clause requires an interface or constraint type");
                    if (requirement.name.empty()) {
                        break;
                    }
                    constraint.requirements.push_back(std::move(requirement));

                    if (is(TokenKind::plus)) {
                        advance();
                        continue;
                    }
                    break;
                }

                if (constraint.requirements.empty()) {
                    break;
                }

                where_constraints.push_back(std::move(constraint));
                if (is(TokenKind::comma)) {
                    advance();
                    continue;
                }

                if (!is(TokenKind::newline) && !is(TokenKind::eof)) {
                    result.diagnostics.error(current().line, "expected ',' or end of where clause");
                }
                break;
            }
        }

        return InterfaceMethodSyntax {
            .name = std::move(name),
            .generic_parameters = std::move(generic_parameters),
            .parameters = std::move(parameters),
            .return_type = std::move(return_type),
            .where_constraints = std::move(where_constraints),
        };
    }

    auto parse_function_body(ParseResult& result, InterfaceMethodSyntax signature, Visibility visibility)
        -> std::optional<FunctionSyntax> {
        FunctionSyntax function {
            .visibility = visibility,
            .name = std::move(signature.name),
            .generic_parameters = std::move(signature.generic_parameters),
            .parameters = std::move(signature.parameters),
            .return_type = std::move(signature.return_type),
            .where_constraints = std::move(signature.where_constraints),
            .body_statements = {},
        };

        auto body = parse_statement_block(result, "function declaration requires an indented body block");
        if (body.empty() && result.diagnostics.has_errors()) {
            return std::nullopt;
        }

        function.body_statements = std::move(body);
        return function;
    }

    auto parse_method_visibility() -> Visibility {
        if (!is_visibility_modifier(current().kind)) {
            return Visibility::package_visibility;
        }

        auto visibility = visibility_from_token(current().kind);
        advance();
        return visibility;
    }

    auto precedence(TokenKind kind) const -> int {
        switch (kind) {
        case TokenKind::keyword_or:
            return 1;
        case TokenKind::keyword_and:
            return 2;
        case TokenKind::keyword_bit_or:
            return 3;
        case TokenKind::keyword_bit_xor:
            return 4;
        case TokenKind::keyword_bit_and:
            return 5;
        case TokenKind::equal_equal:
        case TokenKind::bang_equal:
            return 6;
        case TokenKind::less_equal:
        case TokenKind::greater_equal:
        case TokenKind::less:
        case TokenKind::greater:
            return 7;
        case TokenKind::keyword_shift_left:
        case TokenKind::keyword_shift_right:
            return 8;
        case TokenKind::plus:
        case TokenKind::minus:
            return 10;
        case TokenKind::star:
        case TokenKind::percent:
        case TokenKind::slash:
            return 20;
        default:
            return -1;
        }
    }

    auto make_name_expression(std::string text) -> ExpressionSyntax {
        return ExpressionSyntax {.kind = ExpressionKind::name, .text = std::move(text)};
    }

    auto make_integer_expression(std::string text) -> ExpressionSyntax {
        return ExpressionSyntax {.kind = ExpressionKind::integer_literal, .text = std::move(text)};
    }

    auto make_string_expression(std::string text) -> ExpressionSyntax {
        return ExpressionSyntax {.kind = ExpressionKind::string_literal, .text = std::move(text)};
    }

    auto make_boolean_expression(std::string text) -> ExpressionSyntax {
        return ExpressionSyntax {.kind = ExpressionKind::boolean_literal, .text = std::move(text)};
    }

    auto parse_prefix_expression(ParseResult& result) -> ExpressionSyntax {
        if (is(TokenKind::minus) || is(TokenKind::keyword_not) || is(TokenKind::keyword_bit_not)) {
            auto operator_text = current().lexeme;
            advance();
            auto operand = parse_prefix_expression(result);
            if (operand.text.empty() && !operand.left && !operand.right && operand.arguments.empty()) {
                result.diagnostics.error(current().line, "unary operator requires an operand expression");
                return {};
            }

            return ExpressionSyntax {
                .kind = ExpressionKind::unary,
                .text = std::move(operator_text),
                .arguments = {},
                .left = std::make_unique<ExpressionSyntax>(std::move(operand)),
                .right = nullptr,
                .alternate = nullptr,
            };
        }

        return parse_primary_expression(result);
    }

    auto parse_cast_expression(ParseResult& result) -> ExpressionSyntax {
        auto expression = parse_prefix_expression(result);
        if (expression.text.empty() && !expression.left && !expression.right && expression.arguments.empty()) {
            return expression;
        }

        while (is(TokenKind::keyword_as)) {
            advance();
            auto cast_type = parse_type(result, "cast expression requires a target type after 'as'");
            if (cast_type.name.empty()) {
                return {};
            }

            expression = ExpressionSyntax {
                .kind = ExpressionKind::cast,
                .text = cast_type.name,
                .arguments = {},
                .left = std::make_unique<ExpressionSyntax>(std::move(expression)),
                .right = nullptr,
                .alternate = nullptr,
            };
        }

        return expression;
    }

    auto parse_argument_list(ParseResult& result) -> std::vector<ExpressionSyntax> {
        std::vector<ExpressionSyntax> arguments;
        if (is(TokenKind::right_paren)) {
            advance();
            return arguments;
        }

        while (!is(TokenKind::eof)) {
            auto argument = parse_expression(result);
            if (argument.text.empty() && argument.kind == ExpressionKind::name && !argument.left && !argument.right &&
                argument.arguments.empty()) {
                break;
            }
            arguments.push_back(std::move(argument));

            if (is(TokenKind::comma)) {
                advance();
                continue;
            }

            if (is(TokenKind::right_paren)) {
                advance();
                return arguments;
            }

            result.diagnostics.error(current().line, "expected ',' or ')' in call argument list");
            break;
        }

        if (is(TokenKind::right_paren)) {
            advance();
        } else {
            result.diagnostics.error(current().line, "call expression cannot end before ')'");
        }
        return arguments;
    }

    auto parse_array_literal(ParseResult& result) -> ExpressionSyntax {
        ExpressionSyntax expression {.kind = ExpressionKind::array_literal, .text = ""};
        advance();

        if (is(TokenKind::right_bracket)) {
            advance();
            return expression;
        }

        while (!is(TokenKind::eof)) {
            auto element = parse_expression(result);
            if (element.text.empty() && element.kind == ExpressionKind::name && !element.left && !element.right &&
                element.arguments.empty()) {
                return {};
            }
            expression.arguments.push_back(std::move(element));

            if (is(TokenKind::comma)) {
                advance();
                continue;
            }

            if (is(TokenKind::right_bracket)) {
                advance();
                return expression;
            }

            result.diagnostics.error(current().line, "expected ',' or ']' in array literal");
            return {};
        }

        result.diagnostics.error(current().line, "array literal cannot end before ']'");
        return {};
    }

    auto parse_primary_expression(ParseResult& result) -> ExpressionSyntax {
        if (is(TokenKind::left_bracket)) {
            return parse_array_literal(result);
        }

        if (is(TokenKind::identifier)) {
            auto expression = make_name_expression(current().lexeme);
            advance();

            while (true) {
                if (is(TokenKind::left_paren)) {
                    advance();
                    ExpressionSyntax call_expression {
                        .kind = ExpressionKind::call,
                        .text = "",
                        .arguments = parse_argument_list(result),
                        .left = std::make_unique<ExpressionSyntax>(std::move(expression)),
                        .right = nullptr,
                        .alternate = nullptr,
                    };
                    expression = std::move(call_expression);
                    continue;
                }

                if (is(TokenKind::dot)) {
                    advance();
                    auto member_name = expect_identifier(result, "expected member name after '.'");
                    ExpressionSyntax member_expression {
                        .kind = ExpressionKind::member_access,
                        .text = std::move(member_name),
                        .arguments = {},
                        .left = std::make_unique<ExpressionSyntax>(std::move(expression)),
                        .right = nullptr,
                        .alternate = nullptr,
                    };
                    expression = std::move(member_expression);
                    continue;
                }

                if (is(TokenKind::question_dot)) {
                    advance();
                    auto member_name = expect_identifier(result, "expected member name after '?.'");
                    ExpressionSyntax member_expression {
                        .kind = ExpressionKind::null_safe_member_access,
                        .text = std::move(member_name),
                        .arguments = {},
                        .left = std::make_unique<ExpressionSyntax>(std::move(expression)),
                        .right = nullptr,
                        .alternate = nullptr,
                    };
                    expression = std::move(member_expression);
                    continue;
                }

                if (is(TokenKind::left_bracket)) {
                    advance();
                    auto index_expression = parse_expression(result);
                    if (index_expression.text.empty() && !index_expression.left && !index_expression.right &&
                        index_expression.arguments.empty()) {
                        return {};
                    }

                    if (!is(TokenKind::right_bracket)) {
                        result.diagnostics.error(current().line, "expected ']' after index expression");
                        return {};
                    }

                    advance();
                    std::vector<ExpressionSyntax> index_arguments;
                    index_arguments.push_back(std::move(index_expression));
                    ExpressionSyntax index_access_expression {
                        .kind = ExpressionKind::index_access,
                        .text = "",
                        .arguments = std::move(index_arguments),
                        .left = std::make_unique<ExpressionSyntax>(std::move(expression)),
                        .right = nullptr,
                        .alternate = nullptr,
                    };
                    expression = std::move(index_access_expression);
                    continue;
                }

                break;
            }

            return expression;
        }

        if (is(TokenKind::integer_literal)) {
            auto expression = make_integer_expression(current().lexeme);
            advance();
            return expression;
        }

        if (is(TokenKind::string_literal)) {
            auto expression = make_string_expression(current().lexeme);
            advance();
            return expression;
        }

        if (is(TokenKind::keyword_true) || is(TokenKind::keyword_false)) {
            auto expression = make_boolean_expression(current().lexeme);
            advance();
            return expression;
        }

        result.diagnostics.error(current().line, "expected expression");
        return {};
    }

    auto parse_expression(ParseResult& result, int minimum_precedence = 0) -> ExpressionSyntax {
        auto left = parse_cast_expression(result);
        if (left.text.empty() && !left.left && !left.right && left.arguments.empty()) {
            return left;
        }

        while (true) {
            auto current_precedence = precedence(current().kind);
            if (current_precedence < minimum_precedence) {
                break;
            }

            auto operator_text = current().lexeme;
            advance();
            auto right = parse_expression(result, current_precedence + 1);
            if (right.text.empty() && !right.left && !right.right && right.arguments.empty()) {
                break;
            }

            ExpressionSyntax binary_expression {
                .kind = ExpressionKind::binary,
                .text = std::move(operator_text),
                .arguments = {},
                .left = std::make_unique<ExpressionSyntax>(std::move(left)),
                .right = std::make_unique<ExpressionSyntax>(std::move(right)),
                .alternate = nullptr,
            };
            left = std::move(binary_expression);
        }

        if (minimum_precedence <= 0 && is(TokenKind::question)) {
            advance();
            auto true_expression = parse_expression(result);
            if (true_expression.text.empty() && !true_expression.left && !true_expression.right &&
                !true_expression.alternate && true_expression.arguments.empty()) {
                result.diagnostics.error(current().line, "ternary expression requires a true branch after '?'");
                return {};
            }

            if (!is(TokenKind::colon)) {
                result.diagnostics.error(current().line, "expected ':' after ternary true branch");
                return {};
            }

            advance();
            auto false_expression = parse_expression(result);
            if (false_expression.text.empty() && !false_expression.left && !false_expression.right &&
                !false_expression.alternate && false_expression.arguments.empty()) {
                result.diagnostics.error(current().line, "ternary expression requires a false branch after ':'");
                return {};
            }

            left = ExpressionSyntax {
                .kind = ExpressionKind::ternary,
                .text = "",
                .arguments = {},
                .left = std::make_unique<ExpressionSyntax>(std::move(left)),
                .right = std::make_unique<ExpressionSyntax>(std::move(true_expression)),
                .alternate = std::make_unique<ExpressionSyntax>(std::move(false_expression)),
            };
        }

        return left;
    }

    auto is_assignable_expression(ExpressionSyntax const& expression) const -> bool {
        return expression.kind == ExpressionKind::name || expression.kind == ExpressionKind::member_access;
    }

    auto current_starts_assignment_statement() const -> bool {
        if (!is(TokenKind::identifier)) {
            return false;
        }

        auto lookahead = index_ + 1;
        while (lookahead < tokens_.size()) {
            auto kind = tokens_[lookahead].kind;
            if (kind == TokenKind::dot) {
                ++lookahead;
                if (lookahead >= tokens_.size() || tokens_[lookahead].kind != TokenKind::identifier) {
                    return false;
                }
                ++lookahead;
                continue;
            }

            return kind == TokenKind::equal;
        }

        return false;
    }

    auto parse_binding_statement(ParseResult& result, StatementKind kind) -> StatementSyntax {
        StatementSyntax statement {.kind = kind, .valid = true};
        advance();

        statement.name = expect_identifier(result, "binding statement requires a name");
        if (statement.name.empty()) {
            statement.valid = false;
            return statement;
        }

        if (is(TokenKind::colon)) {
            advance();
            statement.annotated_type = parse_type(result, "binding type annotation requires a type");
        }

        if (!is(TokenKind::equal)) {
            result.diagnostics.error(current().line, "binding statement requires '=' before the initializer");
            statement.valid = false;
            return statement;
        }

        advance();
        statement.expression = parse_expression(result);
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            statement.expression.arguments.empty()) {
            result.diagnostics.error(current().line, "binding statement requires an initializer expression");
            statement.valid = false;
        }
        return statement;
    }

    auto parse_assignment_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::assignment_statement, .valid = true};
        statement.assignment_target = parse_expression(result);
        if (statement.assignment_target.text.empty() && !statement.assignment_target.left &&
            !statement.assignment_target.right && statement.assignment_target.arguments.empty()) {
            result.diagnostics.error(current().line, "assignment statement requires a target expression");
            statement.valid = false;
            return statement;
        }

        if (!is_assignable_expression(statement.assignment_target)) {
            result.diagnostics.error(current().line, "assignment target must be a name or member access");
            statement.valid = false;
            return statement;
        }

        if (!is(TokenKind::equal)) {
            result.diagnostics.error(current().line, "assignment statement requires '=' before the assigned expression");
            statement.valid = false;
            return statement;
        }

        advance();
        statement.expression = parse_expression(result);
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            statement.expression.arguments.empty()) {
            result.diagnostics.error(current().line, "assignment statement requires an assigned expression");
            statement.valid = false;
        }
        return statement;
    }

    auto parse_return_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::return_statement, .valid = true};
        advance();
        if (is(TokenKind::newline) || is(TokenKind::dedent) || is(TokenKind::eof)) {
            return statement;
        }
        statement.expression = parse_expression(result);
        return statement;
    }

    auto parse_break_statement() -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::break_statement, .valid = true};
        advance();
        return statement;
    }

    auto parse_continue_statement() -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::continue_statement, .valid = true};
        advance();
        return statement;
    }

    auto parse_statement_block(ParseResult& result, std::string const& message) -> std::vector<StatementSyntax> {
        std::vector<StatementSyntax> statements;
        if (!consume_block_start(result, message)) {
            return statements;
        }

        while (!is(TokenKind::dedent) && !is(TokenKind::eof)) {
            if (is(TokenKind::newline)) {
                advance();
                continue;
            }

            auto statement = parse_statement(result);
            if (statement.valid) {
                statements.push_back(std::move(statement));
            }
            if (is(TokenKind::newline)) {
                skip_to_next_line();
            } else if (!is(TokenKind::dedent) && !is(TokenKind::eof) && !current().line_start) {
                skip_to_next_line();
            }
        }

        consume_block_end();
        return statements;
    }

    auto parse_if_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::if_statement, .valid = true};
        advance();

        statement.expression = parse_expression(result);
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            statement.expression.arguments.empty()) {
            result.diagnostics.error(current().line, "if statement requires a condition expression");
            statement.valid = false;
            return statement;
        }

        statement.nested_statements =
            parse_statement_block(result, "if statement requires an indented consequence block");
        if (statement.nested_statements.empty() && result.diagnostics.has_errors()) {
            statement.valid = false;
            return statement;
        }

        if (is(TokenKind::keyword_else)) {
            advance();
            statement.alternate_statements =
                parse_statement_block(result, "else clause requires an indented consequence block");
            if (statement.alternate_statements.empty() && result.diagnostics.has_errors()) {
                statement.valid = false;
            }
        }
        return statement;
    }

    auto parse_switch_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::switch_statement, .valid = true};
        advance();

        statement.expression = parse_expression(result);
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            statement.expression.arguments.empty()) {
            result.diagnostics.error(current().line, "switch statement requires a subject expression");
            statement.valid = false;
            return statement;
        }

        if (!consume_block_start(result, "switch statement requires an indented case block")) {
            statement.valid = false;
            return statement;
        }

        bool saw_default = false;
        while (!is(TokenKind::dedent) && !is(TokenKind::eof)) {
            if (is(TokenKind::newline)) {
                advance();
                continue;
            }

            SwitchCaseSyntax switch_case;
            if (is(TokenKind::keyword_default)) {
                if (saw_default) {
                    result.diagnostics.error(current().line, "switch statement may only contain one default case");
                    switch_case.is_default = true;
                } else {
                    switch_case.is_default = true;
                    saw_default = true;
                }
                advance();
            } else {
                switch_case.pattern = parse_expression(result);
                if (switch_case.pattern.text.empty() && !switch_case.pattern.left && !switch_case.pattern.right &&
                    switch_case.pattern.arguments.empty()) {
                    result.diagnostics.error(current().line, "switch case requires a pattern expression or default");
                    statement.valid = false;
                    skip_to_next_line();
                    continue;
                }
            }

            if (!is(TokenKind::fat_arrow)) {
                result.diagnostics.error(current().line, "switch case requires '=>'");
                statement.valid = false;
                skip_to_next_line();
                continue;
            }

            advance();
            if (is(TokenKind::newline)) {
                auto had_errors_before_block = result.diagnostics.has_errors();
                auto consequences =
                    parse_statement_block(result, "switch case requires an indented consequence block");
                if (consequences.empty()) {
                    statement.valid = false;
                    if (!had_errors_before_block && !result.diagnostics.has_errors()) {
                        result.diagnostics.error(current().line, "switch case requires at least one consequence statement");
                    }
                    continue;
                }

                for (auto& consequence : consequences) {
                    switch_case.statements.push_back(std::make_unique<StatementSyntax>(std::move(consequence)));
                }
                statement.switch_cases.push_back(std::move(switch_case));
                continue;
            }

            if (is(TokenKind::dedent) || is(TokenKind::eof)) {
                result.diagnostics.error(current().line, "switch case requires a consequence statement or indented consequence block");
                statement.valid = false;
                skip_to_next_line();
                continue;
            }

            auto consequence = parse_statement(result);
            if (!consequence.valid) {
                result.diagnostics.error(current().line, "switch case requires a valid consequence statement");
                statement.valid = false;
                skip_to_next_line();
                continue;
            }

            switch_case.statements.push_back(std::make_unique<StatementSyntax>(std::move(consequence)));
            statement.switch_cases.push_back(std::move(switch_case));

            if (is(TokenKind::newline)) {
                skip_to_next_line();
            } else if (!is(TokenKind::dedent) && !is(TokenKind::eof) && !current().line_start) {
                skip_to_next_line();
            }
        }

        consume_block_end();
        if (statement.switch_cases.empty()) {
            result.diagnostics.error(current().line, "switch statement requires at least one case");
            statement.valid = false;
        }
        return statement;
    }

    auto parse_guard_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::guard_statement, .valid = true};
        advance();

        statement.expression = parse_expression(result);
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            statement.expression.arguments.empty()) {
            result.diagnostics.error(current().line, "guard statement requires a condition expression");
            statement.valid = false;
            return statement;
        }

        if (!is(TokenKind::keyword_else)) {
            result.diagnostics.error(current().line, "guard statement requires 'else' before the failure block");
            statement.valid = false;
            return statement;
        }

        advance();
        statement.nested_statements = parse_statement_block(result, "guard else clause requires an indented failure block");
        if (statement.nested_statements.empty() && result.diagnostics.has_errors()) {
            statement.valid = false;
        }
        return statement;
    }

    auto parse_while_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::while_statement, .valid = true};
        advance();

        statement.expression = parse_expression(result);
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            statement.expression.arguments.empty()) {
            result.diagnostics.error(current().line, "while statement requires a condition expression");
            statement.valid = false;
            return statement;
        }

        statement.nested_statements =
            parse_statement_block(result, "while statement requires an indented body block");
        if (statement.nested_statements.empty() && result.diagnostics.has_errors()) {
            statement.valid = false;
        }
        return statement;
    }

    auto parse_repeat_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::repeat_statement, .valid = true};
        advance();

        statement.nested_statements =
            parse_statement_block(result, "repeat statement requires an indented body block");
        if (statement.nested_statements.empty() && result.diagnostics.has_errors()) {
            statement.valid = false;
            return statement;
        }

        if (!is(TokenKind::keyword_while)) {
            result.diagnostics.error(current().line, "repeat statement requires a trailing while condition");
            statement.valid = false;
            return statement;
        }

        advance();
        statement.expression = parse_expression(result);
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            statement.expression.arguments.empty()) {
            result.diagnostics.error(current().line, "repeat statement requires a trailing while condition expression");
            statement.valid = false;
        }
        return statement;
    }

    auto parse_for_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::for_statement, .valid = true};
        advance();

        statement.name = expect_identifier(result, "for statement requires a loop binding name");
        if (statement.name.empty()) {
            statement.valid = false;
            return statement;
        }

        if (!is(TokenKind::keyword_in)) {
            result.diagnostics.error(current().line, "for statement requires 'in' before the iterable expression");
            statement.valid = false;
            return statement;
        }

        advance();
        statement.expression = parse_expression(result);
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            statement.expression.arguments.empty()) {
            result.diagnostics.error(current().line, "for statement requires an iterable expression");
            statement.valid = false;
            return statement;
        }

        statement.nested_statements =
            parse_statement_block(result, "for statement requires an indented body block");
        if (statement.nested_statements.empty() && result.diagnostics.has_errors()) {
            statement.valid = false;
        }
        return statement;
    }

    auto parse_unsafe_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::unsafe_statement, .valid = true};
        advance();

        statement.nested_statements =
            parse_statement_block(result, "unsafe statement requires an indented block");
        if (statement.nested_statements.empty() && result.diagnostics.has_errors()) {
            statement.valid = false;
            return statement;
        }

        if (statement.nested_statements.empty()) {
            result.diagnostics.error(current().line, "unsafe statement requires at least one nested statement");
            statement.valid = false;
        }
        return statement;
    }

    auto parse_defer_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::defer_statement, .valid = true};
        advance();

        statement.nested_statements =
            parse_statement_block(result, "defer statement requires an indented cleanup block");
        if (statement.nested_statements.empty() && result.diagnostics.has_errors()) {
            statement.valid = false;
            return statement;
        }

        if (statement.nested_statements.empty()) {
            result.diagnostics.error(current().line, "defer statement requires at least one cleanup statement");
            statement.valid = false;
        }
        return statement;
    }

    auto parse_expression_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::expression_statement, .valid = true};
        statement.expression = parse_expression(result);
        if (statement.expression.text.empty() && !statement.expression.left && !statement.expression.right &&
            statement.expression.arguments.empty()) {
            statement.valid = false;
        }
        return statement;
    }

    auto parse_statement(ParseResult& result) -> StatementSyntax {
        switch (current().kind) {
        case TokenKind::keyword_let:
            return parse_binding_statement(result, StatementKind::let_binding);
        case TokenKind::keyword_var:
            return parse_binding_statement(result, StatementKind::var_binding);
        case TokenKind::keyword_return:
            return parse_return_statement(result);
        case TokenKind::keyword_break:
            return parse_break_statement();
        case TokenKind::keyword_continue:
            return parse_continue_statement();
        case TokenKind::keyword_switch:
            return parse_switch_statement(result);
        case TokenKind::keyword_guard:
            return parse_guard_statement(result);
        case TokenKind::keyword_if:
            return parse_if_statement(result);
        case TokenKind::keyword_while:
            return parse_while_statement(result);
        case TokenKind::keyword_repeat:
            return parse_repeat_statement(result);
        case TokenKind::keyword_for:
            return parse_for_statement(result);
        case TokenKind::keyword_unsafe:
            return parse_unsafe_statement(result);
        case TokenKind::keyword_defer:
            return parse_defer_statement(result);
        case TokenKind::keyword_else: {
            StatementSyntax statement {.kind = StatementKind::expression_statement, .valid = false};
            result.diagnostics.error(current().line, "else must follow an if consequence block");
            advance();
            return statement;
        }
        case TokenKind::keyword_default: {
            StatementSyntax statement {.kind = StatementKind::expression_statement, .valid = false};
            result.diagnostics.error(current().line, "default must appear inside a switch case");
            advance();
            return statement;
        }
        default:
            if (current_starts_assignment_statement()) {
                return parse_assignment_statement(result);
            }
            return parse_expression_statement(result);
        }
    }

    void parse_package(ParseResult& result) {
        auto line = current().line;
        advance();
        auto package_name = parse_qualified_name(result, "package declaration requires a package name");
        if (package_name.empty()) {
            skip_to_next_line();
            return;
        }

        if (!result.module.package_name.empty()) {
            result.diagnostics.error(line, "duplicate package declaration");
        } else {
            result.module.package_name = package_name;
        }

        skip_to_next_line();
    }

    void parse_import(ParseResult& result) {
        advance();
        if (!consume_block_start(result, "import declaration requires an indented import block")) {
            skip_to_next_top_level();
            return;
        }

        while (!is(TokenKind::dedent) && !is(TokenKind::eof)) {
            if (is(TokenKind::newline)) {
                advance();
                continue;
            }

            ImportSyntax import;
            import.name = expect_identifier(result, "import entry requires a name");
            if (import.name.empty()) {
                skip_to_next_line();
                continue;
            }

            if (is(TokenKind::keyword_as)) {
                advance();
                import.alias = expect_identifier(result, "import alias requires a name");
                if (import.alias.empty()) {
                    skip_to_next_line();
                    continue;
                }
            }

            if (!is(TokenKind::keyword_from)) {
                result.diagnostics.error(current().line, "import entry requires 'from' before the package name");
                skip_to_next_line();
                continue;
            }

            advance();
            import.from_package = parse_qualified_name(result, "import entry requires a package name after 'from'");
            if (import.from_package.empty()) {
                skip_to_next_line();
                continue;
            }

            result.module.imports.push_back(std::move(import));
            skip_to_next_line();
        }

        consume_block_end();
    }

    void parse_type_alias(ParseResult& result, Visibility visibility) {
        advance();
        auto name = expect_identifier(result, "type alias declaration requires a name");
        if (name.empty()) {
            skip_to_next_line();
            return;
        }

        if (!is(TokenKind::equal)) {
            result.diagnostics.error(current().line, "type alias declaration requires '=' before the aliased type");
            skip_to_next_line();
            return;
        }

        advance();
        auto aliased_type = parse_type(result, "type alias declaration requires a target type");
        if (aliased_type.name.empty()) {
            skip_to_next_line();
            return;
        }

        result.module.type_aliases.push_back(TypeAliasSyntax {
            .visibility = visibility,
            .name = std::move(name),
            .aliased_type = std::move(aliased_type),
        });
        ++result.module.top_level_declaration_count;
        skip_to_next_line();
    }

    auto parse_field_visibility() -> Visibility {
        if (!is_visibility_modifier(current().kind)) {
            return Visibility::private_visibility;
        }

        auto visibility = visibility_from_token(current().kind);
        advance();
        return visibility;
    }

    void parse_record(ParseResult& result, Visibility visibility) {
        advance();
        auto name = expect_identifier(result, "record declaration requires a type name");
        if (name.empty()) {
            skip_to_next_line();
            return;
        }

        RecordSyntax record {.visibility = visibility, .name = name, .generic_parameters = parse_generic_parameter_names(result)};

        if (!consume_block_start(result, "record declaration requires an indented field block")) {
            skip_to_next_top_level();
            return;
        }

        while (!is(TokenKind::dedent) && !is(TokenKind::eof)) {
            if (is(TokenKind::newline)) {
                advance();
                continue;
            }

            FieldSyntax field;
            field.visibility = parse_field_visibility();
            field.name = expect_identifier(result, "record field requires a name");
            if (field.name.empty()) {
                skip_to_next_line();
                continue;
            }

            if (!is(TokenKind::colon)) {
                result.diagnostics.error(current().line, "record field requires ':' after the field name");
                skip_to_next_line();
                continue;
            }

            advance();
            field.type = parse_type(result, "record field requires a type");
            if (field.type.name.empty()) {
                skip_to_next_line();
                continue;
            }

            record.fields.push_back(std::move(field));
            skip_to_next_line();
        }

        consume_block_end();
        result.module.records.push_back(std::move(record));
        ++result.module.top_level_declaration_count;
    }

    void parse_choice(ParseResult& result, Visibility visibility) {
        advance();
        auto name = expect_identifier(result, "choice declaration requires a type name");
        if (name.empty()) {
            skip_to_next_line();
            return;
        }

        ChoiceSyntax choice {.visibility = visibility, .name = name, .generic_parameters = parse_generic_parameter_names(result)};
        if (!consume_block_start(result, "choice declaration requires an indented variant block")) {
            skip_to_next_top_level();
            return;
        }

        while (!is(TokenKind::dedent) && !is(TokenKind::eof)) {
            if (is(TokenKind::newline)) {
                advance();
                continue;
            }

            ChoiceVariantSyntax variant;
            variant.name = expect_identifier(result, "choice variant requires a name");
            if (variant.name.empty()) {
                skip_to_next_line();
                continue;
            }

            if (is(TokenKind::left_paren)) {
                advance();
                if (!is(TokenKind::right_paren)) {
                    while (!is(TokenKind::eof)) {
                        auto payload = parse_named_type(
                            result,
                            "choice payload requires a field name",
                            "choice payload requires a field type"
                        );
                        if (payload.name.empty()) {
                            break;
                        }
                        variant.payloads.push_back(std::move(payload));

                        if (is(TokenKind::comma)) {
                            advance();
                            continue;
                        }

                        if (is(TokenKind::right_paren)) {
                            break;
                        }

                        result.diagnostics.error(current().line, "expected ',' or ')' in choice payload list");
                        break;
                    }
                }

                if (is(TokenKind::right_paren)) {
                    advance();
                } else {
                    result.diagnostics.error(current().line, "choice payload list cannot end before ')'");
                    skip_to_next_line();
                    continue;
                }
            }

            choice.variants.push_back(std::move(variant));
            skip_to_next_line();
        }

        consume_block_end();
        result.module.choices.push_back(std::move(choice));
        ++result.module.top_level_declaration_count;
    }

    void parse_interface(ParseResult& result, Visibility visibility) {
        advance();
        auto name = expect_identifier(result, "interface declaration requires a name");
        if (name.empty()) {
            skip_to_next_line();
            return;
        }

        InterfaceSyntax interface_syntax {
            .visibility = visibility,
            .name = name,
            .generic_parameters = parse_generic_parameter_names(result),
            .methods = {},
        };

        if (!consume_block_start(result, "interface declaration requires an indented member block")) {
            skip_to_next_top_level();
            return;
        }

        while (!is(TokenKind::dedent) && !is(TokenKind::eof)) {
            if (is(TokenKind::newline)) {
                advance();
                continue;
            }

            auto maybe_method = parse_function_signature(result, "interface members must be function declarations");
            if (!maybe_method.has_value()) {
                skip_to_next_line();
                continue;
            }

            interface_syntax.methods.push_back(std::move(*maybe_method));
            skip_to_next_line();
        }

        consume_block_end();
        result.module.interfaces.push_back(std::move(interface_syntax));
        ++result.module.top_level_declaration_count;
    }

    void parse_implementation(ParseResult& result) {
        advance();
        auto interface_type = parse_type(result, "implements declaration requires an interface type");
        if (interface_type.name.empty()) {
            skip_to_next_line();
            return;
        }

        if (!is(TokenKind::keyword_for)) {
            result.diagnostics.error(current().line, "implements declaration requires 'for' before the receiver type");
            skip_to_next_line();
            return;
        }

        advance();
        auto receiver_type = parse_type(result, "implements declaration requires a receiver type");
        if (receiver_type.name.empty()) {
            skip_to_next_line();
            return;
        }

        ImplementationSyntax implementation {
            .interface_type = std::move(interface_type),
            .receiver_type = std::move(receiver_type),
            .methods = {},
        };

        if (!consume_block_start(result, "implements declaration requires an indented method block")) {
            skip_to_next_top_level();
            return;
        }

        while (!is(TokenKind::dedent) && !is(TokenKind::eof)) {
            if (is(TokenKind::newline)) {
                advance();
                continue;
            }

            auto maybe_signature = parse_function_signature(result, "implements members must be function declarations");
            if (!maybe_signature.has_value()) {
                skip_to_next_line();
                continue;
            }

            auto maybe_method = parse_function_body(result, std::move(*maybe_signature), Visibility::package_visibility);
            if (!maybe_method.has_value()) {
                skip_to_next_top_level();
                return;
            }

            implementation.methods.push_back(std::move(*maybe_method));
        }

        consume_block_end();
        result.module.implementations.push_back(std::move(implementation));
        ++result.module.top_level_declaration_count;
    }

    void parse_extension(ParseResult& result) {
        advance();
        auto receiver_type = parse_type(result, "extend declaration requires a receiver type");
        if (receiver_type.name.empty()) {
            skip_to_next_line();
            return;
        }

        ExtensionSyntax extension {
            .receiver_type = std::move(receiver_type),
            .methods = {},
        };

        if (!consume_block_start(result, "extend declaration requires an indented method block")) {
            skip_to_next_top_level();
            return;
        }

        while (!is(TokenKind::dedent) && !is(TokenKind::eof)) {
            if (is(TokenKind::newline)) {
                advance();
                continue;
            }

            auto visibility = parse_method_visibility();
            auto maybe_signature = parse_function_signature(result, "extend members must be function declarations");
            if (!maybe_signature.has_value()) {
                skip_to_next_line();
                continue;
            }

            auto maybe_method = parse_function_body(result, std::move(*maybe_signature), visibility);
            if (!maybe_method.has_value()) {
                skip_to_next_top_level();
                return;
            }

            extension.methods.push_back(std::move(*maybe_method));
        }

        consume_block_end();
        result.module.extensions.push_back(std::move(extension));
        ++result.module.top_level_declaration_count;
    }

    void parse_function(ParseResult& result, Visibility visibility) {
        auto maybe_signature = parse_function_signature(result, "function declaration requires 'function'");
        if (!maybe_signature.has_value()) {
            skip_to_next_top_level();
            return;
        }

        auto maybe_function = parse_function_body(result, std::move(*maybe_signature), visibility);
        if (!maybe_function.has_value()) {
            skip_to_next_top_level();
            return;
        }

        result.module.functions.push_back(std::move(*maybe_function));
        ++result.module.top_level_declaration_count;
    }

    std::vector<Token> tokens_;
    std::size_t index_ = 0;
};

}  // namespace

auto ModuleParser::parse(source::SourceFile const& source_file) const -> ParseResult {
    Lexer lexer;
    auto lex_result = lexer.lex(source_file);

    Parser parser(std::move(lex_result.tokens));
    auto parse_result = parser.parse();
    for (auto const& diagnostic : lex_result.diagnostics.entries()) {
        parse_result.diagnostics.error(diagnostic.line, diagnostic.message);
    }
    return parse_result;
}

}  // namespace orison::syntax
