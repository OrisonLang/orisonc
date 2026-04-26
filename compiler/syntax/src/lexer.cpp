#include "orison/syntax/lexer.hpp"

#include <cctype>
#include <string_view>
#include <vector>

namespace orison::syntax {
namespace {

auto keyword_kind(std::string_view text) -> TokenKind {
    if (text == "package") {
        return TokenKind::keyword_package;
    }
    if (text == "import") {
        return TokenKind::keyword_import;
    }
    if (text == "type") {
        return TokenKind::keyword_type;
    }
    if (text == "record") {
        return TokenKind::keyword_record;
    }
    if (text == "choice") {
        return TokenKind::keyword_choice;
    }
    if (text == "interface") {
        return TokenKind::keyword_interface;
    }
    if (text == "function") {
        return TokenKind::keyword_function;
    }
    if (text == "public") {
        return TokenKind::keyword_public;
    }
    if (text == "private") {
        return TokenKind::keyword_private;
    }
    if (text == "from") {
        return TokenKind::keyword_from;
    }
    if (text == "as") {
        return TokenKind::keyword_as;
    }
    if (text == "shared") {
        return TokenKind::keyword_shared;
    }
    if (text == "exclusive") {
        return TokenKind::keyword_exclusive;
    }
    if (text == "let") {
        return TokenKind::keyword_let;
    }
    if (text == "var") {
        return TokenKind::keyword_var;
    }
    if (text == "return") {
        return TokenKind::keyword_return;
    }
    if (text == "switch") {
        return TokenKind::keyword_switch;
    }
    if (text == "default") {
        return TokenKind::keyword_default;
    }
    if (text == "guard") {
        return TokenKind::keyword_guard;
    }
    if (text == "if") {
        return TokenKind::keyword_if;
    }
    if (text == "else") {
        return TokenKind::keyword_else;
    }
    if (text == "while") {
        return TokenKind::keyword_while;
    }
    if (text == "for") {
        return TokenKind::keyword_for;
    }
    if (text == "in") {
        return TokenKind::keyword_in;
    }
    if (text == "defer") {
        return TokenKind::keyword_defer;
    }
    return TokenKind::identifier;
}

}  // namespace

auto Lexer::lex(source::SourceFile const& source_file) const -> LexResult {
    LexResult result;
    auto const& input = source_file.content();

    std::size_t index = 0;
    std::size_t line = 1;
    std::size_t column = 1;
    bool at_line_start = true;
    bool first_token_on_line = false;
    std::vector<std::size_t> indent_stack {0};

    auto push_simple_token = [&](TokenKind kind, std::string lexeme, std::size_t token_line, std::size_t token_column) {
        result.tokens.push_back(Token {
            .kind = kind,
            .lexeme = std::move(lexeme),
            .line = token_line,
            .column = token_column,
            .indent = 0,
            .line_start = first_token_on_line,
        });
        first_token_on_line = false;
    };

    while (index < input.size()) {
        char ch = input[index];

        if (at_line_start) {
            std::size_t indent = 0;
            while (index < input.size() && input[index] == ' ') {
                ++indent;
                ++index;
                ++column;
            }

            if (index < input.size() && input[index] == '\t') {
                result.diagnostics.error(line, "tabs are not permitted in indentation");
                while (index < input.size() && input[index] == '\t') {
                    ++index;
                    ++column;
                }
            }

            if (index >= input.size()) {
                break;
            }

            ch = input[index];

            if (ch == '\r') {
                ++index;
                continue;
            }

            if (ch == '\n') {
                push_simple_token(TokenKind::newline, "\n", line, column);
                ++index;
                ++line;
                column = 1;
                continue;
            }

            if (ch == '#') {
                while (index < input.size() && input[index] != '\n') {
                    ++index;
                    ++column;
                }
                continue;
            }

            if (indent > indent_stack.back()) {
                indent_stack.push_back(indent);
                result.tokens.push_back(Token {
                    .kind = TokenKind::indent,
                    .lexeme = "",
                    .line = line,
                    .column = 1,
                    .indent = indent,
                    .line_start = false,
                });
            } else {
                while (indent < indent_stack.back()) {
                    indent_stack.pop_back();
                    result.tokens.push_back(Token {
                        .kind = TokenKind::dedent,
                        .lexeme = "",
                        .line = line,
                        .column = 1,
                        .indent = indent,
                        .line_start = false,
                    });
                }

                if (indent != indent_stack.back()) {
                    result.diagnostics.error(line, "inconsistent indentation level");
                }
            }

            at_line_start = false;
            first_token_on_line = true;
            ch = input[index];
        }

        if (ch == '\r') {
            ++index;
            continue;
        }

        if (ch == '\n') {
            push_simple_token(TokenKind::newline, "\n", line, column);
            ++index;
            ++line;
            column = 1;
            at_line_start = true;
            first_token_on_line = false;
            continue;
        }

        if (ch == '#') {
            while (index < input.size() && input[index] != '\n') {
                ++index;
                ++column;
            }
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
            ++index;
            ++column;
            continue;
        }

        Token token {
            .kind = TokenKind::unknown,
            .lexeme = "",
            .line = line,
            .column = column,
            .indent = 0,
            .line_start = first_token_on_line,
        };
        first_token_on_line = false;

        if (std::isalpha(static_cast<unsigned char>(ch)) != 0 || ch == '_') {
            auto start = index;
            while (index < input.size()) {
                char part = input[index];
                if (std::isalnum(static_cast<unsigned char>(part)) == 0 && part != '_') {
                    break;
                }
                ++index;
                ++column;
            }

            token.lexeme = input.substr(start, index - start);
            token.kind = keyword_kind(token.lexeme);
            result.tokens.push_back(token);
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            auto start = index;
            while (index < input.size() && std::isdigit(static_cast<unsigned char>(input[index])) != 0) {
                ++index;
                ++column;
            }

            token.lexeme = input.substr(start, index - start);
            token.kind = TokenKind::integer_literal;
            result.tokens.push_back(token);
            continue;
        }

        if (ch == '-' && index + 1 < input.size() && input[index + 1] == '>') {
            token.kind = TokenKind::arrow;
            token.lexeme = "->";
            index += 2;
            column += 2;
            result.tokens.push_back(token);
            continue;
        }

        if (ch == '=' && index + 1 < input.size() && input[index + 1] == '>') {
            token.kind = TokenKind::fat_arrow;
            token.lexeme = "=>";
            index += 2;
            column += 2;
            result.tokens.push_back(token);
            continue;
        }

        if (ch == '=' && index + 1 < input.size() && input[index + 1] == '=') {
            token.kind = TokenKind::equal_equal;
            token.lexeme = "==";
            index += 2;
            column += 2;
            result.tokens.push_back(token);
            continue;
        }

        if (ch == '!' && index + 1 < input.size() && input[index + 1] == '=') {
            token.kind = TokenKind::bang_equal;
            token.lexeme = "!=";
            index += 2;
            column += 2;
            result.tokens.push_back(token);
            continue;
        }

        if (ch == '<' && index + 1 < input.size() && input[index + 1] == '=') {
            token.kind = TokenKind::less_equal;
            token.lexeme = "<=";
            index += 2;
            column += 2;
            result.tokens.push_back(token);
            continue;
        }

        if (ch == '>' && index + 1 < input.size() && input[index + 1] == '=') {
            token.kind = TokenKind::greater_equal;
            token.lexeme = ">=";
            index += 2;
            column += 2;
            result.tokens.push_back(token);
            continue;
        }

        switch (ch) {
        case '(':
            token.kind = TokenKind::left_paren;
            break;
        case ')':
            token.kind = TokenKind::right_paren;
            break;
        case ',':
            token.kind = TokenKind::comma;
            break;
        case ':':
            token.kind = TokenKind::colon;
            break;
        case '.':
            token.kind = TokenKind::dot;
            break;
        case '<':
            token.kind = TokenKind::less;
            break;
        case '>':
            token.kind = TokenKind::greater;
            break;
        case '=':
            token.kind = TokenKind::equal;
            break;
        case '+':
            token.kind = TokenKind::plus;
            break;
        case '-':
            token.kind = TokenKind::minus;
            break;
        case '*':
            token.kind = TokenKind::star;
            break;
        case '%':
            token.kind = TokenKind::percent;
            break;
        case '/':
            token.kind = TokenKind::slash;
            break;
        default:
            token.kind = TokenKind::unknown;
            token.lexeme = std::string(1, ch);
            result.tokens.push_back(token);
            result.diagnostics.error(line, "unexpected character in source");
            ++index;
            ++column;
            continue;
        }

        token.lexeme = std::string(1, ch);
        result.tokens.push_back(token);
        ++index;
        ++column;
    }

    while (indent_stack.size() > 1) {
        indent_stack.pop_back();
        result.tokens.push_back(Token {
            .kind = TokenKind::dedent,
            .lexeme = "",
            .line = line,
            .column = 1,
            .indent = 0,
            .line_start = false,
        });
    }

    result.tokens.push_back(Token {
        .kind = TokenKind::eof,
        .lexeme = "",
        .line = line,
        .column = column,
        .indent = 0,
        .line_start = false,
    });
    return result;
}

}  // namespace orison::syntax
