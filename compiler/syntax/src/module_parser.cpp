#include "orison/syntax/module_parser.hpp"

#include "orison/syntax/lexer.hpp"

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

    auto parse_type_name(ParseResult& result) -> std::string {
        auto name = expect_identifier(result, "expected return type after '->'");
        if (name.empty()) {
            return name;
        }

        if (is(TokenKind::less)) {
            name += "<";
            advance();

            bool first = true;
            while (!is(TokenKind::greater) && !is(TokenKind::eof) && !is(TokenKind::newline)) {
                if (!first) {
                    if (is(TokenKind::comma)) {
                        name += ", ";
                        advance();
                    } else {
                        result.diagnostics.error(current().line, "expected ',' or '>' in generic type");
                        break;
                    }
                }

                auto part = parse_qualified_name(result, "expected generic type argument");
                name += part;
                first = false;
            }

            if (is(TokenKind::greater)) {
                name += ">";
                advance();
            } else {
                result.diagnostics.error(current().line, "expected '>' to close generic type");
            }
        }

        return name;
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

            auto field_name = expect_identifier(result, "record field requires a name");
            if (field_name.empty()) {
                skip_to_next_line();
                continue;
            }

            if (!is(TokenKind::colon)) {
                result.diagnostics.error(current().line, "record field requires ':' after the field name");
                skip_to_next_line();
                continue;
            }

            advance();
            auto field_type = parse_type_name(result);
            if (field_type.empty()) {
                skip_to_next_line();
                continue;
            }

            record.field_names.push_back(field_name);
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

        if (!is(TokenKind::left_paren)) {
            result.diagnostics.error(current().line, "expected '(' after function name");
            skip_to_next_line();
            return;
        }

        int depth = 0;
        while (!is(TokenKind::eof)) {
            if (is(TokenKind::left_paren)) {
                ++depth;
            } else if (is(TokenKind::right_paren)) {
                --depth;
                if (depth == 0) {
                    advance();
                    break;
                }
            } else if (is(TokenKind::newline)) {
                result.diagnostics.error(current().line, "function header cannot end before ')'");
                skip_to_next_line();
                return;
            }
            advance();
        }

        if (!is(TokenKind::arrow)) {
            result.diagnostics.error(current().line, "expected '->' after function parameter list");
            skip_to_next_line();
            return;
        }

        advance();
        auto return_type = parse_type_name(result);
        if (return_type.empty()) {
            skip_to_next_top_level();
            return;
        }

        FunctionSyntax function {
            .name = name,
            .return_type = return_type,
            .body_statement_count = 0,
        };

        if (!consume_block_start(result, "function declaration requires an indented body block")) {
            skip_to_next_top_level();
            return;
        }

        while (!is(TokenKind::dedent) && !is(TokenKind::eof)) {
            if (is(TokenKind::newline)) {
                advance();
                continue;
            }

            ++function.body_statement_count;
            skip_to_next_line();
        }

        consume_block_end();
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
