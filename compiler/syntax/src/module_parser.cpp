#include "orison/syntax/module_parser.hpp"

#include "orison/syntax/lexer.hpp"

#include <memory>
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

            switch (token.kind) {
            case TokenKind::keyword_package:
                parse_package(result);
                break;
            case TokenKind::keyword_record:
                parse_record(result);
                break;
            case TokenKind::keyword_function:
                parse_function(result);
                break;
            default:
                result.diagnostics.error(
                    token.line,
                    "expected a top-level declaration such as package, record, or function"
                );
                skip_to_next_line();
                break;
            }
        }

        if (result.module.package_name.empty()) {
            result.diagnostics.error(1, "source file must start with a package declaration");
        }

        return result;
    }

private:
    auto current() const -> Token const& {
        return tokens_[index_];
    }

    auto is(TokenKind kind) const -> bool {
        return current().kind == kind;
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

    auto parse_qualified_name(ParseResult& result, std::string const& message) -> std::string {
        auto name = expect_identifier(result, message);
        while (is(TokenKind::dot)) {
            advance();
            auto part = expect_identifier(result, "expected identifier after '.' in qualified name");
            if (part.empty()) {
                break;
            }
            name += ".";
            name += part;
        }
        return name;
    }

    auto parse_type(ParseResult& result, std::string const& message) -> TypeSyntax {
        TypeSyntax type {.name = parse_qualified_name(result, message)};
        if (type.name.empty()) {
            return type;
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

    auto precedence(TokenKind kind) const -> int {
        switch (kind) {
        case TokenKind::star:
        case TokenKind::slash:
            return 20;
        case TokenKind::plus:
        case TokenKind::minus:
            return 10;
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

    auto parse_primary_expression(ParseResult& result) -> ExpressionSyntax {
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
                    };
                    expression = std::move(member_expression);
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

        result.diagnostics.error(current().line, "expected expression");
        return {};
    }

    auto parse_expression(ParseResult& result, int minimum_precedence = 0) -> ExpressionSyntax {
        auto left = parse_primary_expression(result);
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
            };
            left = std::move(binary_expression);
        }

        return left;
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

    auto parse_return_statement(ParseResult& result) -> StatementSyntax {
        StatementSyntax statement {.kind = StatementKind::return_statement, .valid = true};
        advance();
        if (is(TokenKind::newline) || is(TokenKind::dedent) || is(TokenKind::eof)) {
            return statement;
        }
        statement.expression = parse_expression(result);
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
        case TokenKind::keyword_if:
            return parse_if_statement(result);
        case TokenKind::keyword_else: {
            StatementSyntax statement {.kind = StatementKind::expression_statement, .valid = false};
            result.diagnostics.error(current().line, "else must follow an if consequence block");
            advance();
            return statement;
        }
        default:
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

    void parse_record(ParseResult& result) {
        advance();
        auto name = expect_identifier(result, "record declaration requires a type name");
        if (name.empty()) {
            skip_to_next_line();
            return;
        }

        RecordSyntax record {.name = name};

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

    void parse_function(ParseResult& result) {
        advance();
        auto name = expect_identifier(result, "function declaration requires a name");
        if (name.empty()) {
            skip_to_next_line();
            return;
        }

        auto parameters = parse_parameter_list(result);
        if (result.diagnostics.has_errors() && !is(TokenKind::arrow) && !is(TokenKind::newline)) {
            skip_to_next_line();
        }

        if (!is(TokenKind::arrow)) {
            result.diagnostics.error(current().line, "expected '->' after function parameter list");
            skip_to_next_line();
            return;
        }

        advance();
        auto return_type = parse_type(result, "expected return type after '->'");
        if (return_type.name.empty()) {
            skip_to_next_top_level();
            return;
        }

        FunctionSyntax function {
            .name = name,
            .parameters = std::move(parameters),
            .return_type = std::move(return_type),
            .body_statements = {},
        };

        auto body = parse_statement_block(result, "function declaration requires an indented body block");
        if (body.empty() && result.diagnostics.has_errors()) {
            skip_to_next_top_level();
            return;
        }

        function.body_statements = std::move(body);
        result.module.functions.push_back(std::move(function));
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
