#pragma once

#include <cstddef>
#include <string>

namespace orison::syntax {

enum class TokenKind {
    identifier,
    integer_literal,
    keyword_package,
    keyword_record,
    keyword_function,
    keyword_let,
    keyword_var,
    keyword_return,
    keyword_switch,
    keyword_default,
    keyword_guard,
    keyword_if,
    keyword_else,
    keyword_while,
    keyword_for,
    keyword_in,
    keyword_defer,
    indent,
    dedent,
    left_paren,
    right_paren,
    comma,
    colon,
    dot,
    less,
    less_equal,
    greater,
    greater_equal,
    equal,
    equal_equal,
    bang_equal,
    plus,
    minus,
    star,
    slash,
    arrow,
    fat_arrow,
    newline,
    eof,
    unknown,
};

struct Token {
    TokenKind kind = TokenKind::unknown;
    std::string lexeme;
    std::size_t line = 0;
    std::size_t column = 0;
    std::size_t indent = 0;
    bool line_start = false;
};

}  // namespace orison::syntax
