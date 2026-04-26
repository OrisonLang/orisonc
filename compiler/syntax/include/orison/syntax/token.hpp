#pragma once

#include <cstddef>
#include <string>

namespace orison::syntax {

enum class TokenKind {
    identifier,
    integer_literal,
    string_literal,
    keyword_package,
    keyword_import,
    keyword_type,
    keyword_record,
    keyword_choice,
    keyword_interface,
    keyword_implements,
    keyword_extend,
    keyword_function,
    keyword_public,
    keyword_private,
    keyword_from,
    keyword_as,
    keyword_shared,
    keyword_exclusive,
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
    keyword_break,
    keyword_continue,
    keyword_repeat,
    keyword_unsafe,
    keyword_where,
    keyword_and,
    keyword_or,
    keyword_not,
    keyword_bit_and,
    keyword_bit_or,
    keyword_bit_xor,
    keyword_bit_not,
    keyword_shift_left,
    keyword_shift_right,
    keyword_true,
    keyword_false,
    indent,
    dedent,
    left_bracket,
    right_bracket,
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
    percent,
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
