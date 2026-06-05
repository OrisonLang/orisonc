#include "orison/semantics/module_semantic_analyzer.hpp"
#include "orison/source/source_file.hpp"
#include "orison/syntax/module_parser.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

void write_boxed_maybe_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(inner: Maybe<T>)\n";
    output << "function classify(item: Boxed<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_boxed_pair_maybe_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice PairMaybe<T>\n";
    output << "    PairSome(left: T, right: Int64)\n";
    output << "    Empty\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(inner: PairMaybe<T>)\n";
    output << "function classify(item: Boxed<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    output << "        default => 0\n";
}

void write_boxed_outer_maybe_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "choice Outer<T>\n";
    output << "    Hold(inner: Maybe<T>)\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(inner: Outer<T>)\n";
    output << "function classify(item: Boxed<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    output << "        default => 0\n";
}

void write_list_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool async_function = false,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice List<T>\n";
    output << "    Empty\n";
    output << "    Node(head: T, tail: Box<List<T>>)\n";
    output << (async_function ? "async " : "") << "function sum(xs: List<Int64>) -> Int64\n";
    output << "    switch xs\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_result_switch_with_maybe_variant_fixture(
    std::filesystem::path const& path,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    None\n";
    output << "    Some(value: T)\n";
    output << "choice Result<T>\n";
    output << "    Ok(value: T)\n";
    output << "    Error\n";
    output << "function read(result: Result<Int64>) -> Int64\n";
    output << "    switch result\n";
    output << "        Some(value) => value\n";
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_envelope_result_switch_with_maybe_variant_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    None\n";
    output << "    Some(value: T)\n";
    output << "choice Result<T>\n";
    output << "    Ok(value: T)\n";
    output << "    Error\n";
    output << "choice Envelope<T>\n";
    output << "    Wrap(inner: Result<T>)\n";
    output << "function read(env: Envelope<Int64>) -> Int64\n";
    output << "    switch env\n";
    output << "        Wrap(Some(value)) => value\n";
    output << "        default => 0\n";
}

void write_flag_switch_with_maybe_same_name_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "choice Flag\n";
    output << "    Some\n";
    output << "function read(flag: Flag) -> Int64\n";
    output << "    switch flag\n";
    output << "        Some => 1\n";
}

void write_envelope_pair_flag_switch_with_maybe_same_name_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "choice PairFlag\n";
    output << "    Some(left: Int64, right: Int64)\n";
    output << "choice Envelope\n";
    output << "    Wrap(inner: PairFlag)\n";
    output << "function read(env: Envelope) -> Int64\n";
    output << "    switch env\n";
    output << "        Wrap(Some(left, right)) => left\n";
    output << "        default => 0\n";
}

void write_maybe_raw_write_fixture(std::filesystem::path const& path, std::string_view maybe_payload_type) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    None\n";
    output << "    Some(value: T)\n";
    output << "unsafe function write_word(maybe: Maybe<" << maybe_payload_type;
    output << ">, out: Pointer<UInt32>) -> Unit\n";
    output << "    switch maybe\n";
    output << "        Some(value) => raw_write(out, value)\n";
    output << "        default => return\n";
}

void write_maybe_unknown_constructor_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "function read(item: Maybe<Int64>) -> Int64\n";
    output << "    switch item\n";
    output << "        Missing(value) => value\n";
}

void write_switch_name_pattern_async_capture_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "async function read(value: Int64) -> Int64\n";
    output << "    var head = 0\n";
    output << "    switch value\n";
    output << "        head =>\n";
    output << "            let request_task = task\n";
    output << "                head\n";
    output << "            return await request_task\n";
    output << "        default => 0\n";
}

void write_switch_unknown_call_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "async function read(value: Int64) -> Int64\n";
    output << "    switch value\n";
    output << "        Missing(head) => 0\n";
    output << "        default => 0\n";
}

void write_nested_list_raw_write_fixture(std::filesystem::path const& path, std::string_view list_payload_type) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice List<T>\n";
    output << "    Empty\n";
    output << "    Node(head: T, tail: Box<List<T>>)\n";
    output << "unsafe function write_next(xs: List<" << list_payload_type;
    output << ">, out: Pointer<UInt32>) -> Unit\n";
    output << "    switch xs\n";
    output << "        Node(head, Node(next, tail)) => raw_write(out, next)\n";
    output << "        default => return\n";
}

void write_nested_list_async_capture_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice List<T>\n";
    output << "    Empty\n";
    output << "    Node(head: T, tail: Box<List<T>>)\n";
    output << "async function sum(xs: List<Int64>) -> Int64\n";
    output << "    switch xs\n";
    output << "        Node(head, Node(next, tail)) =>\n";
    output << "            let request_task = task\n";
    output << "                next\n";
    output << "            return await request_task\n";
    output << "        default => 0\n";
}

void write_list_async_head_capture_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice List<T>\n";
    output << "    Empty\n";
    output << "    Node(head: T, tail: Box<List<T>>)\n";
    output << "async function sum(xs: List<Int64>) -> Int64\n";
    output << "    var head = 0\n";
    output << "    switch xs\n";
    output << "        Empty => 0\n";
    output << "        Node(head, tail) =>\n";
    output << "            let request_task = task\n";
    output << "                head\n";
    output << "            return await request_task\n";
}

void write_maybe_choice_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "function classify(item: Maybe<Int64>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 1\n";
    }
}

void write_maybe_int_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice MaybeInt\n";
    output << "    Some(value: Int64)\n";
    output << "    Empty\n";
    output << "function classify(item: MaybeInt) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 2\n";
    }
}

void write_value_then_constructor_pattern_mix_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.patterns\n";
    output << "choice MaybeInt\n";
    output << "    Empty\n";
    output << "    Some(value: Int64)\n";
    output << "function classify(flag: Bool) -> Int64\n";
    output << "    switch flag\n";
    output << "        true => 1\n";
    output << "        Some(value) => value\n";
    output << "        default => 0\n";
}

void write_bool_switch_text_value_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(flag: Bool) -> Int64\n";
    output << "    switch flag\n";
    output << "        \"ready\" => 1\n";
    output << "        default => 0\n";
}

void write_same_width_integer_value_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(value: UInt32) -> Int64\n";
    output << "    switch value\n";
    output << "        1 as Int32 => 1\n";
    output << "        default => 0\n";
}

void write_duplicate_bool_value_pattern_fixture(std::filesystem::path const& path, bool include_default = true) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(flag: Bool) -> Int64\n";
    output << "    switch flag\n";
    output << "        true => 1\n";
    output << "        true => 2\n";
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_duplicate_text_value_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(state: Text) -> Int64\n";
    output << "    switch state\n";
    output << "        \"ready\" => 1\n";
    output << "        \"ready\" => 2\n";
    output << "        default => 0\n";
}

void write_duplicate_integer_cast_value_pattern_fixture(std::filesystem::path const& path) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(value: UInt32) -> Int64\n";
    output << "    switch value\n";
    output << "        1 => 1\n";
    output << "        1 as Int32 => 2\n";
    output << "        default => 0\n";
}

void write_bool_value_pattern_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "function classify(flag: Bool) -> Int64\n";
    output << "    switch flag\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 2\n";
    }
}

void write_zero_payload_choice_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice IOError\n";
    output << "    Closed\n";
    output << "    EndOfInput\n";
    output << "    PermissionDenied\n";
    output << "function classify(error: IOError) -> Int64\n";
    output << "    switch error\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_boxed_maybe_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << "choice Boxed<T>\n";
    output << "    Wrap(value: T)\n";
    output << "    Blank\n";
    output << "function classify(item: Boxed<Maybe<Int64>>) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 1\n";
    }
}

void write_pair_choice_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice PairChoice\n";
    output << "    Both(left: Int64, right: Int64)\n";
    output << "    Empty\n";
    output << "function classify(item: PairChoice) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 2\n";
    }
}

void write_number_choice_switch_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = true
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice Number\n";
    output << "    Int(value: Int64)\n";
    output << "    Empty\n";
    output << "function classify(item: Number) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_multi_payload_choice_exhaustiveness_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> arms,
    bool include_default = false
) {
    std::ofstream output(path);
    output << "package demo.switches\n";
    output << "choice MultiPayload\n";
    output << "    First(value: Int64)\n";
    output << "    Second(value: Int64)\n";
    output << "    Empty\n";
    output << "function classify(item: MultiPayload) -> Int64\n";
    output << "    switch item\n";
    for (auto arm : arms) {
        output << "        " << arm << "\n";
    }
    if (include_default) {
        output << "        default => 0\n";
    }
}

void write_loop_control_fixture(
    std::filesystem::path const& path,
    std::string_view function_header,
    std::initializer_list<std::string_view> body_lines
) {
    std::ofstream output(path);
    output << "package demo.loops\n";
    output << "function " << function_header << "\n";
    for (auto line : body_lines) {
        output << "    " << line << "\n";
    }
}

void write_receiver_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> lines
) {
    std::ofstream output(path);
    output << "package demo.receiver\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

void write_concurrency_fixture(
    std::filesystem::path const& path,
    std::string_view package_name,
    std::initializer_list<std::string_view> lines
) {
    std::ofstream output(path);
    output << "package " << package_name << "\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

void write_concurrency_fixture(
    std::filesystem::path const& path,
    std::string_view package_name,
    std::vector<std::string> const& lines
) {
    std::ofstream output(path);
    output << "package " << package_name << "\n";
    for (auto const& line : lines) {
        output << line << "\n";
    }
}

auto low_level_read_call(std::string_view intrinsic, std::string_view operand) -> std::string {
    return std::string(intrinsic) + "(" + std::string(operand) + ")";
}

auto low_level_final_if_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return {
        std::string(function_line),
        "    if flag",
        "        " + std::string(true_branch),
        "    else",
        "        " + std::string(false_branch),
    };
}

auto low_level_final_switch_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return {
        std::string(function_line),
        "    switch flag",
        "        true => " + std::string(true_branch),
        "        false => " + std::string(false_branch),
    };
}

auto low_level_unsafe_final_if_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return {
        std::string(function_line),
        "    unsafe",
        "        if flag",
        "            " + std::string(true_branch),
        "        else",
        "            " + std::string(false_branch),
    };
}

auto low_level_unsafe_final_switch_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return {
        std::string(function_line),
        "    unsafe",
        "        switch flag",
        "            true => " + std::string(true_branch),
        "            false => " + std::string(false_branch),
    };
}

auto low_level_final_if_unsafe_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return {
        std::string(function_line),
        "    if flag",
        "        unsafe",
        "            " + std::string(true_branch),
        "    else",
        "        " + std::string(false_branch),
    };
}

auto low_level_final_switch_unsafe_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return {
        std::string(function_line),
        "    switch flag",
        "        true =>",
        "            unsafe",
        "                " + std::string(true_branch),
        "        false => " + std::string(false_branch),
    };
}

auto ordinary_final_if_lines(std::string_view true_branch, std::string_view false_branch) -> std::vector<std::string> {
    return {
        "function demo(flag: Bool) -> UInt32",
        "    if flag",
        "        " + std::string(true_branch),
        "    else",
        "        " + std::string(false_branch),
    };
}

auto ordinary_final_switch_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return {
        "function demo(flag: Bool) -> UInt32",
        "    switch flag",
        "        true => " + std::string(true_branch),
        "        false => " + std::string(false_branch),
    };
}

auto choice_final_if_lines(std::string_view true_branch, std::string_view false_branch) -> std::vector<std::string> {
    return {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function demo(flag: Bool) -> Maybe<UInt32>",
        "    if flag",
        "        " + std::string(true_branch),
        "    else",
        "        " + std::string(false_branch),
    };
}

auto choice_final_switch_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function demo(flag: Bool) -> Maybe<UInt32>",
        "    switch flag",
        "        true => " + std::string(true_branch),
        "        false => " + std::string(false_branch),
    };
}

auto choice_final_unsafe_block_ternary_lines(std::string_view expression) -> std::vector<std::string> {
    return {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function demo(flag: Bool) -> Maybe<UInt32>",
        "    unsafe",
        "        " + std::string(expression),
    };
}

auto choice_final_if_ternary_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return choice_final_if_lines(
        "flag ? " + std::string(true_branch) + " : " + std::string(false_branch),
        false_branch
    );
}

auto choice_final_switch_ternary_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return choice_final_switch_lines(
        "flag ? " + std::string(true_branch) + " : " + std::string(false_branch),
        false_branch
    );
}

auto low_level_final_read_direct_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
        "    " + low_level_read_call(intrinsic, "p"),
    };
}

auto low_level_final_read_direct_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
        "    " + low_level_read_call(intrinsic, "p"),
    };
}

auto low_level_final_read_rebound_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(p: Pointer<Byte>) -> UInt32",
        "    let value = " + low_level_read_call(intrinsic, "p"),
        "    value",
    };
}

auto low_level_final_read_rebound_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
        "    let value = " + low_level_read_call(intrinsic, "p"),
        "    value",
    };
}

auto low_level_final_read_unsafe_block_direct_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
        "    unsafe",
        "        " + low_level_read_call(intrinsic, "p"),
    };
}

auto low_level_final_read_unsafe_block_direct_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "function read_byte(p: Pointer<Byte>) -> Byte",
        "    unsafe",
        "        " + low_level_read_call(intrinsic, "p"),
    };
}

auto low_level_final_read_unsafe_block_rebound_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "function read_word(p: Pointer<Byte>) -> UInt32",
        "    unsafe",
        "        let value = " + low_level_read_call(intrinsic, "p"),
        "        value",
    };
}

auto low_level_final_read_unsafe_block_rebound_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "function read_byte(p: Pointer<Byte>) -> Byte",
        "    unsafe",
        "        let value = " + low_level_read_call(intrinsic, "p"),
        "        value",
    };
}

auto low_level_final_read_branch_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> UInt32",
        "    var value = " + low_level_read_call(intrinsic, "left"),
        "    if flag",
        "        value = " + low_level_read_call(intrinsic, "left"),
        "    else",
        "        value = " + low_level_read_call(intrinsic, "right"),
        "    value",
    };
}

auto low_level_final_read_branch_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        "    var value = " + low_level_read_call(intrinsic, "left"),
        "    if flag",
        "        value = " + low_level_read_call(intrinsic, "left"),
        "    else",
        "        value = " + low_level_read_call(intrinsic, "right"),
        "    value",
    };
}

auto low_level_final_read_switch_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(selector: UInt32, left: Pointer<Byte>, right: Pointer<Byte>) -> UInt32",
        "    var value = " + low_level_read_call(intrinsic, "left"),
        "    switch selector",
        "        0 => value = " + low_level_read_call(intrinsic, "left"),
        "        default => value = " + low_level_read_call(intrinsic, "right"),
        "    value",
    };
}

auto low_level_final_read_switch_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_byte(selector: UInt32, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        "    var value = " + low_level_read_call(intrinsic, "left"),
        "    switch selector",
        "        0 => value = " + low_level_read_call(intrinsic, "left"),
        "        default => value = " + low_level_read_call(intrinsic, "right"),
        "    value",
    };
}

auto low_level_final_if_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_if_lines(
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_if_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_if_lines(
        "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        low_level_read_call(intrinsic, "left"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_switch_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_lines(
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_switch_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_lines(
        "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        low_level_read_call(intrinsic, "left"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_unsafe_block_if_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_unsafe_final_if_lines(
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_unsafe_block_if_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_unsafe_final_if_lines(
        "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        low_level_read_call(intrinsic, "left"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_unsafe_block_switch_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_unsafe_final_switch_lines(
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_unsafe_block_switch_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_unsafe_final_switch_lines(
        "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        low_level_read_call(intrinsic, "left"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_if_unsafe_block_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_if_unsafe_lines(
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_if_unsafe_block_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        "    if flag",
        "        unsafe",
        "            " + low_level_read_call(intrinsic, "left"),
        "    else",
        "        unsafe",
        "            " + low_level_read_call(intrinsic, "right"),
    };
}

auto low_level_final_switch_unsafe_block_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_unsafe_lines(
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_switch_unsafe_block_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        "    switch flag",
        "        true =>",
        "            unsafe",
        "                " + low_level_read_call(intrinsic, "left"),
        "        false =>",
        "            unsafe",
        "                " + low_level_read_call(intrinsic, "right"),
    };
}

auto low_level_final_unsafe_block_ternary_read_mismatch_lines(std::string_view intrinsic)
    -> std::vector<std::string> {
    return {
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        "    unsafe",
        "        flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
    };
}

auto low_level_final_unsafe_block_ternary_read_success_lines(std::string_view intrinsic)
    -> std::vector<std::string> {
    return {
        "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        "    unsafe",
        "        flag ? " + low_level_read_call(intrinsic, "left") + " : " +
            low_level_read_call(intrinsic, "right"),
    };
}

auto low_level_final_if_ternary_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_if_lines(
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        "flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
        "1 as UInt32"
    );
}

auto low_level_final_if_ternary_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_if_lines(
        "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        "flag ? " + low_level_read_call(intrinsic, "left") + " : " +
            low_level_read_call(intrinsic, "right"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_switch_ternary_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_lines(
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        "flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
        "1 as UInt32"
    );
}

auto low_level_final_switch_ternary_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_lines(
        "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
        "flag ? " + low_level_read_call(intrinsic, "left") + " : " +
            low_level_read_call(intrinsic, "right"),
        low_level_read_call(intrinsic, "right")
    );
}

auto low_level_final_read_guard_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(flag: Bool, left: Pointer<UInt32>) -> UInt32",
        "    var value = 1 as UInt32",
        "    guard flag else",
        "        value = " + low_level_read_call(intrinsic, "left"),
        "    value",
    };
}

auto low_level_final_read_guard_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        "    var value = " + low_level_read_call(intrinsic, "left"),
        "    guard flag else",
        "        value = " + low_level_read_call(intrinsic, "left"),
        "    value",
    };
}

auto low_level_final_read_while_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(flag: Bool, left: Pointer<UInt32>) -> UInt32",
        "    var value = 1 as UInt32",
        "    while flag",
        "        value = " + low_level_read_call(intrinsic, "left"),
        "    value",
    };
}

auto low_level_final_read_for_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(items: shared View<Int64>, left: Pointer<UInt32>) -> UInt32",
        "    var value = 1 as UInt32",
        "    for item in items",
        "        value = " + low_level_read_call(intrinsic, "left"),
        "    value",
    };
}

auto low_level_final_read_for_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(items: shared View<Int64>, left: Pointer<Byte>) -> UInt32",
        "    var value = " + low_level_read_call(intrinsic, "left"),
        "    for item in items",
        "        value = " + low_level_read_call(intrinsic, "left"),
        "    value",
    };
}

auto low_level_final_read_repeat_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return {
        "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        "    var value = " + low_level_read_call(intrinsic, "left"),
        "    repeat",
        "        value = " + low_level_read_call(intrinsic, "left"),
        "    while flag",
        "    value",
    };
}

void write_status_choice_constant_fixture(std::filesystem::path const& path, std::string_view initializer) {
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "choice Status",
            "    Ready(code: UInt32)",
            "    Empty",
            initializer,
        }
    );
}

void write_maybe_choice_constant_fixture(
    std::filesystem::path const& path,
    std::string_view initializer,
    std::initializer_list<std::string_view> body_lines = {}
) {
    std::ofstream output(path);
    output << "package demo.consts\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    output << initializer << "\n";
    for (auto line : body_lines) {
        output << line << "\n";
    }
}

void write_maybe_choice_constant_fixture_with_declarations(
    std::filesystem::path const& path,
    std::string_view initializer,
    std::initializer_list<std::string_view> declarations,
    std::initializer_list<std::string_view> body_lines = {}
) {
    std::ofstream output(path);
    output << "package demo.consts\n";
    output << "choice Maybe<T>\n";
    output << "    Some(value: T)\n";
    output << "    Empty\n";
    for (auto line : declarations) {
        output << line << "\n";
    }
    output << initializer << "\n";
    for (auto line : body_lines) {
        output << line << "\n";
    }
}

void write_boxed_maybe_choice_constant_fixture(std::filesystem::path const& path, std::string_view initializer) {
    write_maybe_choice_constant_fixture_with_declarations(
        path,
        initializer,
        {
            "choice Boxed<T>",
            "    Wrap(inner: T)",
        }
    );
}

void write_maybe_result_choice_constant_fixture(std::filesystem::path const& path, std::string_view initializer) {
    write_maybe_choice_constant_fixture_with_declarations(
        path,
        initializer,
        {
            "choice Result<T>",
            "    Ok(value: T)",
            "    Error(message: Text)",
        }
    );
}

void write_boxed_maybe_result_choice_constant_fixture(
    std::filesystem::path const& path,
    std::string_view initializer
) {
    write_maybe_choice_constant_fixture_with_declarations(
        path,
        initializer,
        {
            "choice Boxed<T>",
            "    Wrap(inner: T)",
            "choice Result<T>",
            "    Ok(value: T)",
            "    Error(message: Text)",
        }
    );
}

void write_maybe_array_choice_constant_fixture(std::filesystem::path const& path, std::string_view initializer) {
    write_maybe_choice_constant_fixture(path, initializer);
}

void write_boxed_maybe_array_choice_constant_fixture(
    std::filesystem::path const& path,
    std::string_view initializer
) {
    write_maybe_choice_constant_fixture_with_declarations(
        path,
        initializer,
        {
            "choice Boxed<T>",
            "    Wrap(inner: T)",
        }
    );
}

void write_array_constant_fixture(
    std::filesystem::path const& path,
    std::string_view initializer,
    std::initializer_list<std::string_view> body_lines = {}
) {
    std::vector<std::string_view> lines {initializer};
    lines.insert(lines.end(), body_lines.begin(), body_lines.end());
    std::ofstream output(path);
    output << "package demo.consts\n";
    for (auto line : lines) {
        output << line << "\n";
    }
}

void write_header_record_constant_fixture(
    std::filesystem::path const& path,
    std::initializer_list<std::string_view> body_lines
) {
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
        }
    );

    std::ofstream output(path, std::ios::app);
    for (auto line : body_lines) {
        output << line << "\n";
    }
}

auto scalar_array_constant_initializer(std::string_view elements, std::size_t length = 1) -> std::string {
    return "const MAGIC: Array<UInt32, " + std::to_string(length) + "> = [" + std::string(elements) + "]";
}

auto nested_array_constant_initializer(std::string_view elements, std::size_t length = 1) -> std::string {
    return "const MATRIX: Array<Array<UInt32, 2>, " + std::to_string(length) + "> = [" + std::string(elements) + "]";
}

void write_nested_array_constant_fixture(
    std::filesystem::path const& path,
    std::string_view initializer,
    std::initializer_list<std::string_view> body_lines = {}
) {
    write_array_constant_fixture(path, initializer, body_lines);
}

void write_scalar_array_runtime_constant_fixture(
    std::filesystem::path const& path,
    std::string_view element,
    std::initializer_list<std::string_view> body_lines = {}
) {
    write_array_constant_fixture(
        path,
        scalar_array_constant_initializer(element),
        body_lines
    );
}

void write_maybe_array_payload_runtime_constant_fixture(
    std::filesystem::path const& path,
    std::string_view element,
    std::initializer_list<std::string_view> body_lines = {}
) {
    write_maybe_choice_constant_fixture(
        path,
        "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([" + std::string(element) + "])",
        body_lines
    );
}

void write_scalar_array_block_runtime_constant_fixture(
    std::filesystem::path const& path,
    std::string_view construct
) {
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const MAGIC: Array<UInt32, 1> = [" + std::string(construct),
            "    1",
            "]",
        }
    );
}

void write_maybe_array_payload_block_runtime_constant_fixture(
    std::filesystem::path const& path,
    std::string_view construct
) {
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([" + std::string(construct),
            "    1",
            "])",
        }
    );
}

auto box_maybe_items_fixture_lines(
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Box<T>, 1>"
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "choice Wrap<T>",
        "    Items(value: " + std::string(payload_type) + ")",
        "record Box<T>",
        "    value: Maybe<T>",
    };
    if (include_holder) {
        lines.push_back("record Holder<T>");
        lines.push_back("    wrap: Wrap<T>");
    }
    lines.push_back("function demo(flag: Bool) -> UInt32");
    lines.push_back(std::string(binding_line));
    lines.push_back("    return 1");
    return lines;
}

auto slot_pointer_items_fixture_lines(
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "record Slot<T>",
        "    ptr: Pointer<T>",
        "choice Wrap<T>",
        "    Items(value: " + std::string(payload_type) + ")",
    };
    if (include_holder) {
        lines.push_back("record Holder<T>");
        lines.push_back("    wrap: Wrap<T>");
    }
    lines.push_back(
        "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32"
    );
    lines.push_back(std::string(binding_line));
    lines.push_back("    return 1");
    return lines;
}

auto slot_pointer_items_success_fixture_lines(
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
)
    -> std::vector<std::string> {
    auto lines = slot_pointer_items_fixture_lines(binding_line, include_holder, payload_type);
    auto const function_index = include_holder ? std::size_t {6} : std::size_t {4};
    lines[function_index] =
        "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_record_fixture_lines(std::string_view binding_line, bool include_outer) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record Box<T>",
        "    value: Maybe<T>",
    };
    if (include_outer) {
        lines.push_back("record Outer<T>");
        lines.push_back("    box: Box<T>");
    }
    lines.push_back("function demo(flag: Bool) -> UInt32");
    lines.push_back(std::string(binding_line));
    lines.push_back("    return 1");
    return lines;
}

auto slot_pointer_record_fixture_lines(std::string_view binding_line, bool include_wrapper) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "record Slot<T>",
        "    ptr: Pointer<T>",
    };
    if (include_wrapper) {
        lines.push_back("record Wrapper<T>");
        lines.push_back("    slot: Slot<T>");
    }
    lines.push_back(
        "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32"
    );
    lines.push_back(std::string(binding_line));
    lines.push_back("    return 1");
    return lines;
}

auto slot_pointer_record_success_fixture_lines(std::string_view binding_line, bool include_wrapper)
    -> std::vector<std::string> {
    auto lines = slot_pointer_record_fixture_lines(binding_line, include_wrapper);
    auto const function_index = include_wrapper ? std::size_t {4} : std::size_t {2};
    lines[function_index] =
        "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_nested_array_field_fixture_lines(std::string_view binding_line) -> std::vector<std::string> {
    return {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record Box<T>",
        "    value: Maybe<T>",
        "record Shelf<T>",
        "    boxes: Array<Array<Box<T>, 1>, 1>",
        "function demo(flag: Bool) -> UInt32",
        std::string(binding_line),
        "    return 1",
    };
}

auto slot_pointer_nested_array_field_fixture_lines(std::string_view binding_line) -> std::vector<std::string> {
    return {
        "record Slot<T>",
        "    ptr: Pointer<T>",
        "record Rack<T>",
        "    slots: Array<Array<Slot<T>, 1>, 1>",
        "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
        std::string(binding_line),
        "    return 1",
    };
}

auto slot_pointer_nested_array_field_success_fixture_lines(std::string_view binding_line) -> std::vector<std::string> {
    auto lines = slot_pointer_nested_array_field_fixture_lines(binding_line);
    lines[4] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto maybe_nested_array_field_fixture_lines(std::string_view binding_line) -> std::vector<std::string> {
    return {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record ChoiceShelf<T>",
        "    values: Array<Array<Maybe<T>, 1>, 1>",
        "function demo(flag: Bool) -> UInt32",
        std::string(binding_line),
        "    return 1",
    };
}

auto box_maybe_nested_array_argument_fixture_lines(std::string_view call_line) -> std::vector<std::string> {
    return {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record Box<T>",
        "    value: Maybe<T>",
        "function consume(values: Array<Array<Box<UInt32>, 1>, 1>) -> UInt32",
        "    return 1",
        "function demo(flag: Bool) -> UInt32",
        std::string(call_line),
    };
}

auto maybe_nested_array_argument_fixture_lines(std::string_view call_line) -> std::vector<std::string> {
    return {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "function consume(values: Array<Array<Maybe<UInt32>, 1>, 1>) -> UInt32",
        "    return 1",
        "function demo(flag: Bool) -> UInt32",
        std::string(call_line),
    };
}

auto box_maybe_nested_array_method_argument_fixture_lines(std::string_view call_line) -> std::vector<std::string> {
    return {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record Box<T>",
        "    value: Maybe<T>",
        "record Consumer",
        "    id: UInt32",
        "extend Consumer",
        "    function consume(this: shared This, values: Array<Array<Box<UInt32>, 1>, 1>) -> UInt32",
        "        return 1",
        "function demo(consumer: Consumer, flag: Bool) -> UInt32",
        std::string(call_line),
    };
}

auto maybe_nested_array_method_argument_fixture_lines(std::string_view call_line) -> std::vector<std::string> {
    return {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record Consumer",
        "    id: UInt32",
        "extend Consumer",
        "    function consume(this: shared This, values: Array<Array<Maybe<UInt32>, 1>, 1>) -> UInt32",
        "        return 1",
        "function demo(consumer: Consumer, flag: Bool) -> UInt32",
        std::string(call_line),
    };
}

void assert_fixture_success(std::filesystem::path const& path);
void assert_raw_offset_source_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
);
void assert_choice_constructor_payload_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
);

template <typename SourceLines>
void assert_record_field_nested_array_choice_context_failure(
    std::string_view fixture_name,
    SourceLines const& lines,
    int diagnostic_line
) {
    auto path = std::filesystem::temp_directory_path() / std::string(fixture_name);
    write_concurrency_fixture(path, "demo.records", lines);
    assert_choice_constructor_payload_mismatch_diagnostic(path, diagnostic_line, "Bool", "UInt32");
}

template <typename SourceLines>
void assert_record_field_nested_array_pointer_context_failure(
    std::string_view fixture_name,
    SourceLines const& lines,
    int diagnostic_line
) {
    auto path = std::filesystem::temp_directory_path() / std::string(fixture_name);
    write_concurrency_fixture(path, "demo.records", lines);
    assert_raw_offset_source_pointee_mismatch_diagnostic(path, diagnostic_line, "Byte", "UInt32");
}

template <typename SourceLines>
void assert_record_field_nested_array_context_success(
    std::string_view fixture_name,
    SourceLines const& lines
) {
    auto path = std::filesystem::temp_directory_path() / std::string(fixture_name);
    write_concurrency_fixture(path, "demo.records", lines);
    assert_fixture_success(path);
}

auto analyze_orison_fixture(std::filesystem::path const& path) -> orison::semantics::SemanticAnalysisResult {
    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    return analyzer.analyze(parse_result.module);
}

template <typename MutateSwitch>
auto analyze_mutated_first_switch_fixture(
    std::filesystem::path const& path,
    MutateSwitch mutate_switch
) -> orison::semantics::SemanticAnalysisResult {
    auto source_file = orison::source::SourceFile::read(path);
    assert(source_file.has_value());

    orison::syntax::ModuleParser parser;
    auto parse_result = parser.parse(*source_file);
    assert(!parse_result.diagnostics.has_errors());

    auto& switch_statement = parse_result.module.functions.front().body_statements.front();
    assert(switch_statement.kind == orison::syntax::StatementKind::switch_statement);
    mutate_switch(switch_statement);

    orison::semantics::ModuleSemanticAnalyzer analyzer;
    return analyzer.analyze(parse_result.module);
}

void assert_wrap_duplicate_diagnostic(
    orison::semantics::SemanticAnalysisResult const& diagnostics,
    std::size_t expected_line = 10
) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == expected_line);
    assert(diagnostics.entries().front().message == "switch constructor pattern 'Wrap(...)' is duplicated");
}

void assert_fixture_wrap_duplicate_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line = 10
) {
    assert_wrap_duplicate_diagnostic(analyze_orison_fixture(path), expected_line);
}

void assert_single_diagnostic(
    orison::semantics::SemanticAnalysisResult const& diagnostics,
    std::size_t expected_line,
    std::string_view expected_message
) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 1);
    assert(diagnostics.entries().front().line == expected_line);
    assert(diagnostics.entries().front().message == expected_message);
}

std::string this_type_context_message() {
    return "This type is only valid inside interface, implements, or extend methods";
}

std::string switch_nonfinal_default_message() {
    return "switch default case must be the final case";
}

std::string switch_multiple_default_message() {
    return "switch statement may only contain one default case";
}

void assert_nonfinal_switch_default_diagnostic(orison::semantics::SemanticAnalysisResult const& diagnostics) {
    assert_single_diagnostic(diagnostics, 4, switch_nonfinal_default_message());
}

void assert_multiple_switch_default_diagnostics(orison::semantics::SemanticAnalysisResult const& diagnostics) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 2);
    assert(diagnostics.entries().front().line == 4);
    assert(diagnostics.entries().front().message == switch_nonfinal_default_message());
    assert(diagnostics.entries().back().line == 5);
    assert(diagnostics.entries().back().message == switch_multiple_default_message());
}

void assert_this_type_context_diagnostics(
    orison::semantics::SemanticAnalysisResult const& diagnostics,
    std::size_t expected_line,
    std::size_t expected_count
) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == expected_count);
    for (auto const& diagnostic : diagnostics.entries()) {
        assert(diagnostic.line == expected_line);
        assert(diagnostic.message == this_type_context_message());
    }
}

void assert_fixture_single_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view expected_message
) {
    assert_single_diagnostic(analyze_orison_fixture(path), expected_line, expected_message);
}

void assert_fixture_success(std::filesystem::path const& path) {
    assert(!analyze_orison_fixture(path).has_errors());
}

void assert_capture_fields(
    orison::semantics::ConcurrencyCapture const& capture,
    std::string_view expected_name,
    std::optional<std::string_view> expected_type_name,
    std::optional<orison::semantics::ConcurrencyExpressionKind> expected_expression_kind,
    std::optional<orison::semantics::ConcurrencyCaptureKind> expected_capture_kind
) {
    assert(capture.name == expected_name);
    if (expected_type_name.has_value()) {
        assert(capture.type_name == *expected_type_name);
    }
    if (expected_expression_kind.has_value()) {
        assert(capture.expression_kind == *expected_expression_kind);
    }
    if (expected_capture_kind.has_value()) {
        assert(capture.capture_kind == *expected_capture_kind);
    }
}

void assert_fixture_single_capture_fields(
    std::filesystem::path const& path,
    std::string_view expected_name,
    std::optional<std::string_view> expected_type_name,
    std::optional<orison::semantics::ConcurrencyExpressionKind> expected_expression_kind,
    std::optional<orison::semantics::ConcurrencyCaptureKind> expected_capture_kind
) {
    auto diagnostics = analyze_orison_fixture(path);
    assert(!diagnostics.has_errors());
    assert(diagnostics.concurrency_captures.size() == 1);
    assert_capture_fields(
        diagnostics.concurrency_captures.front(),
        expected_name,
        expected_type_name,
        expected_expression_kind,
        expected_capture_kind
    );
}

void assert_fixture_single_capture(std::filesystem::path const& path, std::string_view expected_name) {
    assert_fixture_single_capture_fields(path, expected_name, std::nullopt, std::nullopt, std::nullopt);
}

void assert_fixture_single_capture_kind(
    std::filesystem::path const& path,
    std::string_view expected_name,
    orison::semantics::ConcurrencyCaptureKind expected_kind
) {
    assert_fixture_single_capture_fields(path, expected_name, std::nullopt, std::nullopt, expected_kind);
}

std::string future_marker_type_requirement(
    std::string_view type_name,
    std::string_view marker_name
) {
    return "type '" + std::string(type_name) +
           "' requires future " +
           std::string(marker_name) +
           " analysis";
}

std::string future_marker_analysis_message(
    std::string_view subject,
    std::string_view type_name,
    std::string_view marker_name
) {
    return std::string(subject) + " " +
           future_marker_type_requirement(type_name, marker_name);
}

void assert_future_marker_result_success(std::filesystem::path const& path) {
    assert_fixture_success(path);
}

void assert_future_marker_result_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view subject,
    std::string_view result_type_name,
    std::string_view marker_name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        future_marker_analysis_message(subject, result_type_name, marker_name)
    );
}

void assert_thread_result_transferable_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view result_type_name
) {
    assert_future_marker_result_diagnostic(
        path,
        expected_line,
        "thread result",
        result_type_name,
        "Transferable"
    );
}

void assert_thread_result_transferable_success(std::filesystem::path const& path) {
    assert_future_marker_result_success(path);
}

void assert_task_result_shareable_success(std::filesystem::path const& path) {
    assert_future_marker_result_success(path);
}

std::string thread_capture_transferable_message(
    std::string_view capture_name,
    std::string_view capture_type_name
) {
    return "thread capture '" + std::string(capture_name) + "' of " +
           future_marker_type_requirement(capture_type_name, "Transferable");
}

void assert_thread_capture_transferable_diagnostic(
    orison::semantics::SemanticAnalysisResult const& analysis,
    std::size_t expected_line,
    std::string_view capture_name,
    std::string_view capture_type_name
) {
    assert(analysis.has_errors());
    assert(analysis.entries().size() == 1);
    assert(analysis.entries().front().line == expected_line);
    assert(analysis.entries().front().message ==
           thread_capture_transferable_message(capture_name, capture_type_name));
}

void assert_thread_capture_transferable_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view capture_name,
    std::string_view capture_type_name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        thread_capture_transferable_message(capture_name, capture_type_name)
    );
}

std::string concurrency_value_boundary_message(std::string_view expression_kind) {
    return std::string(expression_kind) + " expression body must end with an expression statement or value return";
}

void assert_concurrency_value_boundary_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view expression_kind
) {
    assert_fixture_single_diagnostic(path, expected_line, concurrency_value_boundary_message(expression_kind));
}

void assert_task_value_boundary_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_concurrency_value_boundary_diagnostic(path, expected_line, "task");
}

void assert_thread_value_boundary_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_concurrency_value_boundary_diagnostic(path, expected_line, "thread");
}

void assert_concurrency_expression_success(std::filesystem::path const& path) {
    assert_fixture_success(path);
}

void assert_task_value_return_success(std::filesystem::path const& path) {
    assert_concurrency_expression_success(path);
}

void assert_thread_value_return_success(std::filesystem::path const& path) {
    assert_concurrency_expression_success(path);
}

std::string concurrency_cannot_capture_message(std::string_view captured_subject) {
    return "concurrency expression cannot capture " + std::string(captured_subject);
}

std::string mutable_outer_local_capture_subject(std::string_view local_name) {
    return "mutable outer local '" + std::string(local_name) + "'";
}

std::string receiver_capture_subject() {
    return "receiver 'this'";
}

void assert_mutable_capture_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view local_name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        concurrency_cannot_capture_message(mutable_outer_local_capture_subject(local_name))
    );
}

void assert_receiver_capture_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(path, expected_line, concurrency_cannot_capture_message(receiver_capture_subject()));
}

std::string switch_constructor_pattern_message(
    std::string_view constructor_pattern,
    std::string_view suffix
) {
    return "switch constructor pattern '" +
           std::string(constructor_pattern) +
           "' " +
           std::string(suffix);
}

std::string switch_constructor_duplicate_binding_message(std::string_view binding_name) {
    return "switch constructor pattern cannot bind '" +
           std::string(binding_name) +
           "' more than once";
}

std::string switch_constructor_arity_suffix(std::size_t expected_count, std::size_t actual_count) {
    return "expects " + std::to_string(expected_count) +
           " payload values but received " +
           std::to_string(actual_count);
}

void assert_switch_unknown_constructor_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view constructor_name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_constructor_pattern_message(constructor_name, "does not match any declared choice variant")
    );
}

void assert_switch_wrong_choice_constructor_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view constructor_name,
    std::string_view choice_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_constructor_pattern_message(
            constructor_name,
            "does not belong to switched choice type '" + std::string(choice_type) + "'"
        )
    );
}

void assert_switch_duplicate_constructor_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view constructor_pattern
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_constructor_pattern_message(constructor_pattern, "is duplicated")
    );
}

void assert_switch_payload_shape_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "switch constructor pattern payload currently requires a binding name, literal, or nested constructor pattern"
    );
}

void assert_switch_duplicate_binding_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view binding_name
) {
    assert_fixture_single_diagnostic(path, expected_line, switch_constructor_duplicate_binding_message(binding_name));
}

void assert_switch_constructor_arity_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view constructor_name,
    std::size_t expected_count,
    std::size_t actual_count
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_constructor_pattern_message(
            constructor_name,
            switch_constructor_arity_suffix(expected_count, actual_count)
        )
    );
}

void assert_switch_pattern_mix_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "switch cannot mix value patterns with constructor patterns"
    );
}

std::string switch_value_pattern_message(std::string_view pattern_text, std::string_view suffix) {
    return "switch value pattern '" +
           std::string(pattern_text) +
           "' " +
           std::string(suffix);
}

std::string switch_value_pattern_type_message(std::string_view pattern_type, std::string_view subject_type) {
    return "switch value pattern type '" +
           std::string(pattern_type) +
           "' does not match switched expression type '" +
           std::string(subject_type) + "'";
}

void assert_switch_value_pattern_type_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view pattern_type,
    std::string_view subject_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_value_pattern_type_message(pattern_type, subject_type)
    );
}

void assert_switch_duplicate_value_pattern_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view pattern_text
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_value_pattern_message(pattern_text, "is duplicated")
    );
}

std::string switch_redundant_default_message(std::string_view covered_subject) {
    return "switch default case is redundant after " + std::string(covered_subject);
}

std::string switch_bool_coverage_subject() {
    return "true and false value patterns";
}

std::string switch_zero_payload_choice_coverage_subject() {
    return "all zero-payload choice variants are covered";
}

std::string switch_choice_coverage_subject() {
    return "all choice variants are covered";
}

std::string switch_missing_message(std::string_view missing_subject, std::string_view missing_name) {
    return "switch is missing " +
           std::string(missing_subject) +
           " '" +
           std::string(missing_name) + "'";
}

void assert_switch_redundant_bool_default_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_redundant_default_message(switch_bool_coverage_subject())
    );
}

void assert_switch_missing_bool_value_pattern_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view missing_pattern
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_missing_message("boolean value pattern", missing_pattern)
    );
}

void assert_switch_redundant_zero_payload_choice_default_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_redundant_default_message(switch_zero_payload_choice_coverage_subject())
    );
}

void assert_switch_redundant_choice_default_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_redundant_default_message(switch_choice_coverage_subject())
    );
}

void assert_switch_missing_choice_variant_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view variant_name
) {
    assert_fixture_single_diagnostic(path, expected_line, switch_missing_message("choice variant", variant_name));
}

void assert_switch_missing_zero_payload_choice_variant_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view variant_name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        switch_missing_message("zero-payload choice variant", variant_name)
    );
}

void assert_nonfinal_switch_default_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(path, expected_line, switch_nonfinal_default_message());
}

std::string loop_control_outside_loop_message(std::string_view statement_name) {
    return std::string(statement_name) + " statement is only valid inside loops";
}

void assert_break_outside_loop_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(path, expected_line, loop_control_outside_loop_message("break"));
}

void assert_continue_outside_loop_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(path, expected_line, loop_control_outside_loop_message("continue"));
}

std::string receiver_context_message(std::string_view receiver_subject, std::string_view location_preposition) {
    return std::string(receiver_subject) +
           " is only valid " +
           std::string(location_preposition) +
           " implements or extend methods";
}

std::string receiver_this_subject() {
    return "receiver 'this'";
}

std::string receiver_parameter_subject() {
    return "receiver parameter 'this'";
}

std::string receiver_parameter_self_type_message() {
    return "receiver parameter 'this' must use This, shared This, or exclusive This";
}

void assert_receiver_this_context_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        receiver_context_message(receiver_this_subject(), "inside")
    );
}

void assert_receiver_parameter_context_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        receiver_context_message(receiver_parameter_subject(), "in")
    );
}

void assert_receiver_parameter_self_type_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        receiver_parameter_self_type_message()
    );
}

void assert_fixture_this_type_context_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_this_type_context_diagnostics(analyze_orison_fixture(path), expected_line, 1);
}

std::string unsafe_boundary_context() {
    return "inside unsafe functions or unsafe blocks";
}

std::string unsafe_intrinsic_context_message(std::string_view intrinsic_name) {
    return "unsafe intrinsic '" +
           std::string(intrinsic_name) +
           "' is only valid " +
           unsafe_boundary_context();
}

std::string unsafe_function_call_context_message(std::string_view function_name) {
    return "call to unsafe function '" +
           std::string(function_name) +
           "' is only valid " +
           unsafe_boundary_context();
}

std::string pointer_construction_unsafe_boundary_message() {
    return "Pointer construction is only valid " + unsafe_boundary_context();
}

void assert_unsafe_intrinsic_context_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view intrinsic_name
) {
    assert_fixture_single_diagnostic(path, expected_line, unsafe_intrinsic_context_message(intrinsic_name));
}

std::string current_requirement_message(std::string_view operation_name, std::string_view requirement) {
    return std::string(operation_name) +
           " currently requires " +
           std::string(requirement);
}

std::string address_like_first_argument_requirement() {
    return "an address-like first argument";
}

std::string structurally_pointer_like_expression_requirement() {
    return "a structurally pointer-like expression";
}

std::string structurally_address_like_expression_requirement() {
    return "a structurally address-like expression";
}

std::string pointer_element_mismatch_message(
    std::string_view operation_name,
    std::string_view actual_type,
    std::string_view expected_type
) {
    return std::string(operation_name) +
           " pointer element type '" +
           std::string(actual_type) +
           "' does not match expected pointer element type '" +
           std::string(expected_type) + "'";
}

std::string value_pointer_element_mismatch_message(
    std::string_view operation_name,
    std::string_view value_type,
    std::string_view pointer_element_type
) {
    return std::string(operation_name) +
           " value type '" +
           std::string(value_type) +
           "' does not match pointer element type '" +
           std::string(pointer_element_type) + "'";
}

std::string result_type_mismatch_message(
    std::string_view operation_name,
    std::string_view result_type,
    std::string_view target_kind,
    std::string_view expected_type
) {
    return std::string(operation_name) +
           " result type '" +
           std::string(result_type) +
           "' does not match " +
           std::string(target_kind) +
           " '" +
           std::string(expected_type) + "'";
}

std::string cannot_action_value_guidance_message(
    std::string_view action,
    std::string_view value_kind,
    std::string_view guidance
) {
    return std::string(action) +
           " cannot " +
           std::string(value_kind) +
           "; " +
           std::string(guidance);
}

void assert_address_of_storage_operand_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("address_of", "an addressable storage operand")
    );
}

void assert_raw_read_address_operand_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("raw_read", address_like_first_argument_requirement())
    );
}

void assert_raw_offset_address_operand_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("raw_offset", address_like_first_argument_requirement())
    );
}

void assert_raw_offset_integer_offset_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("raw_offset", "an integer offset argument")
    );
}

void assert_volatile_read_address_operand_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("volatile_read", address_like_first_argument_requirement())
    );
}

void assert_index_access_integer_index_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("index access", "an integer index expression")
    );
}

void assert_unsafe_function_call_context_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view function_name
) {
    assert_fixture_single_diagnostic(path, expected_line, unsafe_function_call_context_message(function_name));
}

void assert_pointer_construction_unsafe_boundary_diagnostics(
    orison::semantics::SemanticAnalysisResult const& diagnostics
) {
    assert(diagnostics.has_errors());
    assert(diagnostics.entries().size() == 2);
    assert(diagnostics.entries().front().line == 3);
    assert(diagnostics.entries().front().message == pointer_construction_unsafe_boundary_message());
    assert(diagnostics.entries().back().line == 4);
    assert(diagnostics.entries().back().message == unsafe_intrinsic_context_message("raw_read"));
}

void assert_pointer_construction_single_source_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("Pointer construction", "exactly one source argument")
    );
}

void assert_pointer_construction_address_source_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("Pointer construction", "an address-like source argument")
    );
}

void assert_pointer_typed_binding_initializer_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message(
            "pointer-typed binding initializer",
            structurally_pointer_like_expression_requirement()
        )
    );
}

void assert_pointer_typed_constant_initializer_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message(
            "pointer-typed constant initializer",
            structurally_pointer_like_expression_requirement()
        )
    );
}

void assert_pointer_typed_binding_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        pointer_element_mismatch_message("pointer-typed binding initializer", actual_type, expected_type)
    );
}

void assert_pointer_return_structural_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("pointer-returning function", structurally_pointer_like_expression_requirement())
    );
}

void assert_raw_offset_source_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        pointer_element_mismatch_message("raw_offset source", actual_type, expected_type)
    );
}

void assert_pointer_return_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        pointer_element_mismatch_message("pointer-returning function", actual_type, expected_type)
    );
}

void assert_raw_write_value_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view value_type,
    std::string_view pointer_element_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        value_pointer_element_mismatch_message("raw_write", value_type, pointer_element_type)
    );
}

void assert_constant_initializer_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view initializer_type,
    std::string_view declared_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "constant initializer type '" + std::string(initializer_type) +
            "' does not match declared constant type '" + std::string(declared_type) + "'"
    );
}

void assert_constant_array_initializer_length_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::size_t actual_length,
    std::size_t declared_length
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "constant array initializer length " + std::to_string(actual_length) +
            " does not match declared length " + std::to_string(declared_length)
    );
}

void assert_constant_array_initializer_element_type_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view declared_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "constant array initializer element type '" + std::string(actual_type) +
            "' does not match declared element type '" + std::string(declared_type) + "'"
    );
}

void assert_record_constructor_arity_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view record_name,
    std::size_t expected_fields,
    std::size_t actual_fields
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "record constructor '" + std::string(record_name) + "' expects " +
            std::to_string(expected_fields) + " field value" + (expected_fields == 1 ? "" : "s") +
            " but received " + std::to_string(actual_fields)
    );
}

void assert_record_constructor_field_type_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view field_name,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "record constructor field '" + std::string(field_name) + "' type '" +
            std::string(actual_type) + "' does not match expected field type '" +
            std::string(expected_type) + "'"
    );
}

void assert_binding_initializer_type_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "binding initializer type '" + std::string(actual_type) +
            "' does not match declared type '" + std::string(expected_type) + "'"
    );
}

void assert_return_expression_type_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "return expression type '" + std::string(actual_type) +
            "' does not match declared type '" + std::string(expected_type) + "'"
    );
}

void assert_final_expression_type_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "final expression type '" + std::string(actual_type) +
            "' does not match declared type '" + std::string(expected_type) + "'"
    );
}

void assert_function_final_value_required_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "function body must end with an expression statement, value return, or total final-expression container"
    );
}

void assert_empty_return_value_required_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "return statement must return a value for declared type '" + std::string(expected_type) + "'"
    );
}

void assert_assignment_value_type_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "assignment value type '" + std::string(actual_type) +
            "' does not match declared type '" + std::string(expected_type) + "'"
    );
}

void assert_function_argument_type_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view argument_name,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "function argument '" + std::string(argument_name) + "' type '" + std::string(actual_type) +
            "' does not match declared type '" + std::string(expected_type) + "'"
    );
}

void assert_method_argument_type_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view argument_name,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "method argument '" + std::string(argument_name) + "' type '" + std::string(actual_type) +
            "' does not match declared type '" + std::string(expected_type) + "'"
    );
}

void assert_constant_initializer_unknown_name_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "constant initializer references unknown name '" + std::string(name) + "'"
    );
}

void assert_duplicate_top_level_constant_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "top-level constant '" + std::string(name) + "' is duplicated"
    );
}

void assert_constant_initializer_cycle_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "constant initializer cycle includes '" + std::string(name) + "'"
    );
}

void assert_constant_initializer_runtime_construct_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view construct
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "constant initializer cannot use " + std::string(construct)
    );
}

void assert_constant_initializer_unsafe_call_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view function_name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "constant initializer cannot call unsafe function '" + std::string(function_name) + "'"
    );
}

void assert_constant_initializer_function_call_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view function_name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "constant initializer cannot call function '" + std::string(function_name) + "'"
    );
}

void assert_constant_initializer_method_call_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view method_name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "constant initializer cannot call method '" + std::string(method_name) + "'"
    );
}

void assert_choice_constructor_arity_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view constructor_name,
    std::size_t expected_count,
    std::size_t actual_count
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "choice constructor '" + std::string(constructor_name) + "' expects " +
            std::to_string(expected_count) + " payload value" + (expected_count == 1 ? "" : "s") +
            " but received " + std::to_string(actual_count)
    );
}

void assert_choice_constructor_payload_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view actual_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "choice constructor payload type '" + std::string(actual_type) +
            "' does not match expected payload type '" + std::string(expected_type) + "'"
    );
}

void assert_choice_constructor_expected_type_required_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view constructor_name
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "choice constructor '" + std::string(constructor_name) + "' requires an expected choice type"
    );
}

void assert_choice_constructor_declared_type_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view constructor_name,
    std::string_view declared_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "choice constructor '" + std::string(constructor_name) +
            "' does not belong to declared constant type '" + std::string(declared_type) + "'"
    );
}

void assert_choice_constructor_unknown_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view constructor_name,
    std::string_view declared_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "choice constructor '" + std::string(constructor_name) +
            "' does not match any declared choice variant for constant type '" + std::string(declared_type) + "'"
    );
}

void assert_pointer_construction_source_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view source_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        "Pointer construction source type '" + std::string(source_type) +
            "' does not match expected pointer element type '" + std::string(expected_type) + "'"
    );
}

void assert_raw_read_result_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view result_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        result_type_mismatch_message("raw_read", result_type, "function return type", expected_type)
    );
}

void assert_raw_read_binding_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view result_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        result_type_mismatch_message("raw_read", result_type, "binding type", expected_type)
    );
}

void assert_volatile_read_result_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view result_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        result_type_mismatch_message("volatile_read", result_type, "function return type", expected_type)
    );
}

void assert_low_level_read_result_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view intrinsic,
    std::string_view result_type,
    std::string_view expected_type
) {
    if (intrinsic == "raw_read") {
        assert_raw_read_result_mismatch_diagnostic(path, expected_line, result_type, expected_type);
        return;
    }

    assert_volatile_read_result_mismatch_diagnostic(path, expected_line, result_type, expected_type);
}

void assert_volatile_read_binding_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view result_type,
    std::string_view expected_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        result_type_mismatch_message("volatile_read", result_type, "binding type", expected_type)
    );
}

void assert_volatile_write_value_pointee_mismatch_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view value_type,
    std::string_view pointer_element_type
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        value_pointer_element_mismatch_message("volatile_write", value_type, pointer_element_type)
    );
}

void assert_address_binding_initializer_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message(
            "address-typed binding initializer",
            structurally_address_like_expression_requirement()
        )
    );
}

void assert_address_constant_initializer_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message(
            "address-typed constant initializer",
            structurally_address_like_expression_requirement()
        )
    );
}

void assert_address_return_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("address-returning function", structurally_address_like_expression_requirement())
    );
}

std::string concurrency_expression_inside_async_message(std::string_view expression_name) {
    return std::string(expression_name) + " expression is only valid inside async functions";
}

std::string await_async_value_requirement() {
    return "a task value or declared async call result";
}

void assert_concurrency_expression_inside_async_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view expression_name
) {
    assert_fixture_single_diagnostic(path, expected_line, concurrency_expression_inside_async_message(expression_name));
}

void assert_task_inside_async_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_concurrency_expression_inside_async_diagnostic(path, expected_line, "task");
}

void assert_await_inside_async_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_concurrency_expression_inside_async_diagnostic(path, expected_line, "await");
}

void assert_await_requires_async_value_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        current_requirement_message("await expression", await_async_value_requirement())
    );
}

std::string use_join_instead_guidance() {
    return "use .join() instead";
}

std::string use_await_instead_guidance() {
    return "use await instead";
}

std::string thread_value_kind() {
    return "be used with thread values";
}

std::string task_value_kind() {
    return "be used with task values";
}

std::string async_call_value_kind() {
    return "be used with declared async call results";
}

std::string forward_task_or_async_call_value_kind() {
    return "forward task or async-call values";
}

std::string forward_thread_value_kind() {
    return "forward thread values";
}

void assert_use_join_instead_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view action,
    std::string_view value_kind
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        cannot_action_value_guidance_message(action, value_kind, use_join_instead_guidance())
    );
}

void assert_use_await_instead_diagnostic(
    std::filesystem::path const& path,
    std::size_t expected_line,
    std::string_view action,
    std::string_view value_kind
) {
    assert_fixture_single_diagnostic(
        path,
        expected_line,
        cannot_action_value_guidance_message(action, value_kind, use_await_instead_guidance())
    );
}

void assert_await_thread_value_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_use_join_instead_diagnostic(path, expected_line, "await", thread_value_kind());
}

void assert_async_return_forward_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_use_await_instead_diagnostic(path, expected_line, "return", forward_task_or_async_call_value_kind());
}

void assert_join_task_receiver_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_use_await_instead_diagnostic(path, expected_line, "join()", task_value_kind());
}

void assert_join_async_call_receiver_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_use_await_instead_diagnostic(path, expected_line, "join()", async_call_value_kind());
}

void assert_thread_return_forward_diagnostic(std::filesystem::path const& path, std::size_t expected_line) {
    assert_use_join_instead_diagnostic(path, expected_line, "return", forward_thread_value_kind());
}

void assert_concurrency_capture(
    orison::semantics::SemanticAnalysisResult const& analysis,
    std::size_t index,
    std::string_view expected_name,
    std::string_view expected_type_name,
    orison::semantics::ConcurrencyExpressionKind expected_expression_kind,
    orison::semantics::ConcurrencyCaptureKind expected_capture_kind
) {
    assert(analysis.concurrency_captures.size() > index);
    assert_capture_fields(
        analysis.concurrency_captures[index],
        expected_name,
        std::optional<std::string_view>{expected_type_name},
        std::optional<orison::semantics::ConcurrencyExpressionKind>{expected_expression_kind},
        std::optional<orison::semantics::ConcurrencyCaptureKind>{expected_capture_kind}
    );
}

void assert_single_concurrency_capture_success(
    std::filesystem::path const& path,
    std::string_view expected_name,
    std::string_view expected_type_name,
    orison::semantics::ConcurrencyExpressionKind expected_expression_kind,
    orison::semantics::ConcurrencyCaptureKind expected_capture_kind
) {
    assert_fixture_single_capture_fields(
        path,
        expected_name,
        std::optional<std::string_view>{expected_type_name},
        std::optional<orison::semantics::ConcurrencyExpressionKind>{expected_expression_kind},
        std::optional<orison::semantics::ConcurrencyCaptureKind>{expected_capture_kind}
    );
}

void test_await_inside_async_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_async_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return await request_task",
        }
    );

    assert_fixture_success(path);
}

void test_await_async_call_value_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_async_call_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_await_outside_async_function_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_sync_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "function fetch(url: Text) -> Outcome<Text, IOError>",
            "    return await request(url)",
        }
    );

    assert_await_inside_async_diagnostic(path, 5);
}

void test_await_outside_async_method_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_method_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(id: Int64) -> Outcome<Text, IOError>",
            "    return fetch_remote(id)",
            "extend Worker",
            "    function poll(this: shared This) -> Outcome<Text, IOError>",
            "        return await request(this.id)",
        }
    );

    assert_await_inside_async_diagnostic(path, 6);
}

void test_await_plain_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_plain_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch() -> Int64",
            "    let count = 1",
            "    return await count",
        }
    );

    assert_await_requires_async_value_diagnostic(path, 4);
}

void test_await_thread_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_thread_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch() -> Int64",
            "    let worker = thread",
            "        1",
            "    return await worker",
        }
    );

    assert_await_thread_value_diagnostic(path, 5);
}

void test_await_non_async_call_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_await_non_async_call_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return await pending",
        }
    );

    assert_await_requires_async_value_diagnostic(path, 6);
}

void test_await_member_call_not_marked_async_from_top_level_name_collision_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_await_member_name_collision_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function run(text: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(text)",
            "extend Printer",
            "    function run(this: shared This) -> Outcome<Text, IOError>",
            "        return render(this)",
            "async function fetch(printer: Printer) -> Outcome<Text, IOError>",
            "    let pending = printer.run()",
            "    return await pending",
        }
    );

    assert_await_requires_async_value_diagnostic(path, 9);
}

void test_task_inside_async_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_async_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return await request_task",
        }
    );

    assert_fixture_success(path);
}

void test_return_task_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_task_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return request_task",
        }
    );

    assert_async_return_forward_diagnostic(path, 5);
}

void test_return_async_call_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_async_call_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return pending",
        }
    );

    assert_async_return_forward_diagnostic(path, 6);
}

void test_return_thread_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_thread_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum() -> Int64",
            "    let worker = thread",
            "        1",
            "    return worker",
        }
    );

    assert_thread_return_forward_diagnostic(path, 5);
}

void test_assignment_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_assignment_preserves_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_assignment_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_assignment_preserves_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "async function fetch() -> Int64",
            "    var worker = thread",
            "        1",
            "    worker = thread",
            "        2",
            "    return await worker",
        }
    );

    assert_await_thread_value_diagnostic(path, 7);
}

void test_ternary_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_ternary_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    let left = request(url)",
            "    let right = request(url)",
            "    let pending = flag ? left : right",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_ternary_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_ternary_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    let left = thread",
            "        1",
            "    let right = thread",
            "        2",
            "    let worker = flag ? left : right",
            "    return await worker",
        }
    );

    assert_await_thread_value_diagnostic(path, 8);
}

void test_return_ternary_async_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_return_ternary_async_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    let left = request(url)",
            "    let right = request(url)",
            "    return flag ? left : right",
        }
    );

    assert_async_return_forward_diagnostic(path, 7);
}

void test_if_branch_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_if_branch_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    if flag",
            "        pending = request(url)",
            "    else",
            "        pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_if_branch_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_if_branch_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = thread",
            "        1",
            "    if flag",
            "        worker = thread",
            "            2",
            "    else",
            "        worker = thread",
            "            3",
            "    return await worker",
        }
    );

    assert_await_thread_value_diagnostic(path, 11);
}

void test_switch_branch_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_branch_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    switch flag",
            "        true => pending = request(url)",
            "        false => pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_switch_branch_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_branch_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = thread",
            "        1",
            "    switch flag",
            "        true => worker = thread",
            "            2",
            "        false => worker = thread",
            "            3",
            "    return await worker",
        }
    );

    assert_await_thread_value_diagnostic(path, 10);
}

void test_while_loop_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_while_loop_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    while flag",
            "        pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_while_loop_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_while_loop_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = thread",
            "        1",
            "    while flag",
            "        worker = thread",
            "            2",
            "    return await worker",
        }
    );

    assert_await_thread_value_diagnostic(path, 8);
}

void test_repeat_loop_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_repeat_loop_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = 0",
            "    repeat",
            "        pending = request(url)",
            "    while flag",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_repeat_loop_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_repeat_loop_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(flag: Bool) -> Int64",
            "    var worker = 0",
            "    repeat",
            "        worker = thread",
            "            2",
            "    while flag",
            "    return await worker",
        }
    );

    assert_await_thread_value_diagnostic(path, 8);
}

void test_for_loop_preserves_async_call_origin_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_for_loop_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(items: shared View<Int64>, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    for item in items",
            "        pending = request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_for_loop_preserves_thread_origin_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_for_loop_thread_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function fetch(items: shared View<Int64>) -> Int64",
            "    var worker = thread",
            "        1",
            "    for item in items",
            "        worker = thread",
            "            2",
            "    return await worker",
        }
    );

    assert_await_thread_value_diagnostic(path, 8);
}

void test_guard_failure_path_does_not_override_async_origin_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_guard_failure_path_async_origin_success.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = request(url)",
            "    guard flag else",
            "        pending = thread",
            "            2",
            "        return await request(url)",
            "    return await pending",
        }
    );

    assert_fixture_success(path);
}

void test_guard_failure_path_does_not_create_async_origin_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_guard_failure_path_async_origin_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(flag: Bool, url: Text) -> Outcome<Text, IOError>",
            "    var pending = 0",
            "    guard flag else",
            "        pending = request(url)",
            "        return await request(url)",
            "    return await pending",
        }
    );

    assert_await_requires_async_value_diagnostic(path, 9);
}

void test_switch_constructor_pattern_binds_case_local_names_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_binding_success.or";
    write_list_async_head_capture_fixture(path);

    assert_fixture_single_capture_kind(
        path,
        "head",
        orison::semantics::ConcurrencyCaptureKind::immutable_outer_local
    );
}

void test_switch_top_level_name_pattern_rejects_unknown_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_name_pattern_binding_failure.or";
    write_switch_name_pattern_async_capture_fixture(path);

    assert_switch_unknown_constructor_diagnostic(path, 5, "head");
}

void test_switch_call_pattern_rejects_unknown_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_call_pattern_unknown_variant_failure.or";
    write_switch_unknown_call_pattern_fixture(path);

    assert_switch_unknown_constructor_diagnostic(path, 4, "Missing");
}

void test_switch_unknown_constructor_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_unknown_constructor_without_default_no_cascade_failure.or";
    write_maybe_unknown_constructor_fixture(path);

    assert_switch_unknown_constructor_diagnostic(path, 7, "Missing");
}

void test_switch_nested_constructor_pattern_binds_nested_names_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_constructor_pattern_success.or";
    write_nested_list_async_capture_fixture(path);

    assert_fixture_single_capture(path, "next");
}

void test_switch_nested_constructor_pattern_binds_wrapped_payload_type_for_low_level_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wrapped_payload_success.or";
    write_nested_list_raw_write_fixture(path, "Int32");

    assert_fixture_success(path);
}

void test_switch_rejects_nested_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(value)) => 1", "Wrap(Some(other)) => 2"});

    assert_fixture_wrap_duplicate_diagnostic(path);
}

void test_switch_rejects_nested_literal_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(1)) => 1", "Wrap(Some(1)) => 2"});

    assert_fixture_wrap_duplicate_diagnostic(path);
}

void test_switch_accepts_disjoint_nested_literal_payload_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_disjoint_nested_literal_payload_success.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(1)) => 1", "Wrap(Some(2)) => 2"});

    assert_fixture_success(path);
}

void test_switch_rejects_nested_wildcard_literal_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(value)) => 1", "Wrap(Some(1)) => 2"});

    assert_fixture_wrap_duplicate_diagnostic(path);
}

void test_switch_rejects_nested_literal_wildcard_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(1)) => 1", "Wrap(Some(value)) => 2"});

    assert_fixture_wrap_duplicate_diagnostic(path);
}

void test_switch_rejects_nested_multi_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_multi_payload_overlap_failure.or";
    write_boxed_pair_maybe_switch_fixture(
        path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 1)) => 2"}
    );

    assert_fixture_wrap_duplicate_diagnostic(path);
}

void test_switch_accepts_disjoint_nested_multi_payload_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_disjoint_nested_multi_payload_success.or";
    write_boxed_pair_maybe_switch_fixture(
        path,
        {"Wrap(PairSome(left, 1)) => 1", "Wrap(PairSome(other, 2)) => 2"}
    );

    assert_fixture_success(path);
}

void test_switch_accepts_mismatched_nested_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_mismatched_nested_constructor_success.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Some(value)) => 1", "Wrap(Empty) => 2"});

    assert_fixture_success(path);
}

void test_switch_rejects_duplicate_nested_zero_payload_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_nested_zero_payload_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Empty) => 1", "Wrap(Empty) => 2"});

    assert_fixture_wrap_duplicate_diagnostic(path);
}

void test_switch_rejects_duplicate_nested_zero_payload_constructor_no_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_nested_zero_payload_no_cascade_failure.or";
    write_boxed_maybe_switch_fixture(path, {"Wrap(Empty) => 1", "Wrap(Empty) => 2"}, false);

    assert_fixture_wrap_duplicate_diagnostic(path);
}

void test_switch_rejects_deep_nested_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_deep_nested_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(other))) => 2"}
    );

    assert_fixture_wrap_duplicate_diagnostic(path, 12);
}

void test_switch_accepts_disjoint_deep_nested_literal_payload_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_disjoint_deep_nested_literal_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(2))) => 2"}
    );

    assert_fixture_success(path);
}

void test_switch_rejects_deep_nested_wildcard_literal_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_deep_nested_wildcard_literal_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Some(1))) => 2"}
    );

    assert_fixture_wrap_duplicate_diagnostic(path, 12);
}

void test_switch_rejects_deep_nested_literal_wildcard_payload_constructor_overlap_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_deep_nested_literal_wildcard_payload_overlap_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(1))) => 1", "Wrap(Hold(Some(value))) => 2"}
    );

    assert_fixture_wrap_duplicate_diagnostic(path, 12);
}

void test_switch_accepts_mismatched_deep_nested_zero_payload_constructor_patterns_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_mismatched_deep_nested_zero_payload_success.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Some(value))) => 1", "Wrap(Hold(Empty)) => 2"}
    );

    assert_fixture_success(path);
}

void test_switch_rejects_duplicate_deep_nested_zero_payload_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_deep_nested_zero_payload_failure.or";
    write_boxed_outer_maybe_switch_fixture(
        path,
        {"Wrap(Hold(Empty)) => 1", "Wrap(Hold(Empty)) => 2"}
    );

    assert_fixture_wrap_duplicate_diagnostic(path, 12);
}

void test_switch_nested_constructor_pattern_binds_wrapped_payload_type_for_low_level_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wrapped_payload_failure.or";
    write_nested_list_raw_write_fixture(path, "Byte");

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 7, "Byte", "UInt32");
}

void test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_generic_constructor_payload_success.or";
    write_maybe_raw_write_fixture(path, "Int32");

    assert_fixture_success(path);
}

void test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_generic_constructor_payload_failure.or";
    write_maybe_raw_write_fixture(path, "Byte");

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 7, "Byte", "UInt32");
}

void test_switch_constructor_pattern_rejects_variant_from_different_choice_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_wrong_choice_variant_failure.or";
    write_result_switch_with_maybe_variant_fixture(path);

    assert_switch_wrong_choice_constructor_diagnostic(path, 10, "Some", "Result<Int64>");
}

void test_switch_wrong_choice_constructor_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_wrong_choice_constructor_without_default_no_cascade_failure.or";
    write_result_switch_with_maybe_variant_fixture(path, false);

    assert_switch_wrong_choice_constructor_diagnostic(path, 10, "Some", "Result<Int64>");
}

void test_switch_constructor_pattern_uses_subject_specific_arity_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_subject_specific_arity_success.or";
    write_flag_switch_with_maybe_same_name_fixture(path);

    assert_fixture_success(path);
}

void test_switch_nested_constructor_pattern_rejects_variant_from_different_payload_choice_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_wrong_payload_choice_failure.or";
    write_envelope_result_switch_with_maybe_variant_fixture(path);

    assert_switch_wrong_choice_constructor_diagnostic(path, 12, "Some", "Result<Int64>");
}

void test_switch_nested_constructor_pattern_uses_payload_specific_arity_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_payload_specific_arity_success.or";
    write_envelope_pair_flag_switch_with_maybe_same_name_fixture(path);

    assert_fixture_success(path);
}

void test_switch_nested_constructor_pattern_rejects_invalid_payload_shape_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_constructor_pattern_shape_failure.or";
    write_list_switch_fixture(path, {"Node(head + 1, tail) => 0"}, true);

    assert_switch_payload_shape_diagnostic(path, 7);
}

void test_switch_constructor_payload_shape_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_constructor_payload_shape_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Node(head + 1, tail) => 0"}, false, false);

    assert_switch_payload_shape_diagnostic(path, 7);
}

void test_switch_constructor_pattern_rejects_duplicate_binding_names_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_duplicate_binding_failure.or";
    write_list_switch_fixture(path, {"Node(head, head) => 0"}, true);

    assert_switch_duplicate_binding_diagnostic(path, 7, "head");
}

void test_switch_constructor_duplicate_binding_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Node(head, head) => 0"}, false, false);

    assert_switch_duplicate_binding_diagnostic(path, 7, "head");
}

void test_switch_nested_constructor_pattern_rejects_duplicate_binding_names_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_switch_nested_constructor_pattern_duplicate_binding_failure.or";
    write_list_switch_fixture(path, {"Node(head, Node(head, tail)) => 0"}, true);

    assert_switch_duplicate_binding_diagnostic(path, 7, "head");
}

void test_switch_nested_constructor_duplicate_binding_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_nested_constructor_duplicate_binding_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Node(head, Node(head, tail)) => 0"}, false, false);

    assert_switch_duplicate_binding_diagnostic(path, 7, "head");
}

void test_switch_constructor_pattern_rejects_missing_payload_values_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_arity_missing_failure.or";
    write_list_switch_fixture(path, {"Node(head) => 0"}, true);

    assert_switch_constructor_arity_diagnostic(path, 7, "Node", 2, 1);
}

void test_switch_constructor_pattern_rejects_extra_payload_values_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_constructor_pattern_arity_extra_failure.or";
    write_list_switch_fixture(path, {"Empty(value) => 0"}, true);

    assert_switch_constructor_arity_diagnostic(path, 7, "Empty", 0, 1);
}

void test_switch_constructor_pattern_arity_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_constructor_pattern_arity_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Node(head) => 0"}, false, false);

    assert_switch_constructor_arity_diagnostic(path, 7, "Node", 2, 1);
}

void test_switch_zero_payload_constructor_arity_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_zero_payload_constructor_arity_without_default_no_cascade_failure.or";
    write_list_switch_fixture(path, {"Empty(value) => 0"}, false, false);

    assert_switch_constructor_arity_diagnostic(path, 7, "Empty", 0, 1);
}

void test_switch_rejects_constructor_then_value_pattern_mix_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_pattern_mix_constructor_value_failure.or";
    write_list_switch_fixture(path, {"Empty => 0", "1 => 1"});

    assert_switch_pattern_mix_diagnostic(path, 8);
}

void test_switch_rejects_value_then_constructor_pattern_mix_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_pattern_mix_value_constructor_failure.or";
    write_value_then_constructor_pattern_mix_fixture(path);

    assert_switch_pattern_mix_diagnostic(path, 8);
}

void test_switch_pattern_mix_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_pattern_mix_without_default_no_cascade_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value", "1 => 1"});

    assert_switch_pattern_mix_diagnostic(path, 8);
}

void test_switch_rejects_mismatched_value_pattern_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_value_pattern_type_failure.or";
    write_bool_switch_text_value_pattern_fixture(path);

    assert_switch_value_pattern_type_diagnostic(path, 4, "Text", "Bool");
}

void test_switch_accepts_same_width_integer_cast_value_pattern_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_value_pattern_same_width_success.or";
    write_same_width_integer_value_pattern_fixture(path);

    assert_fixture_success(path);
}

void test_switch_rejects_duplicate_boolean_value_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_boolean_value_failure.or";
    write_duplicate_bool_value_pattern_fixture(path);

    assert_switch_duplicate_value_pattern_diagnostic(path, 5, "true");
}

void test_switch_duplicate_bool_without_default_does_not_cascade_to_missing_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_bool_without_default_no_cascade_failure.or";
    write_duplicate_bool_value_pattern_fixture(path, false);

    assert_switch_duplicate_value_pattern_diagnostic(path, 5, "true");
}

void test_switch_rejects_duplicate_string_value_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_string_value_failure.or";
    write_duplicate_text_value_pattern_fixture(path);

    assert_switch_duplicate_value_pattern_diagnostic(path, 5, "\"ready\"");
}

void test_switch_rejects_duplicate_integer_cast_value_pattern_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_integer_cast_value_failure.or";
    write_duplicate_integer_cast_value_pattern_fixture(path);

    assert_switch_duplicate_value_pattern_diagnostic(path, 5, "1 as Int32");
}

void test_switch_rejects_redundant_bool_default_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_redundant_bool_default_failure.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0"}, true);

    assert_switch_redundant_bool_default_diagnostic(path, 6);
}

void test_switch_duplicate_bool_suppresses_redundant_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_bool_redundant_default_no_cascade.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0", "false => 2"}, true);

    assert_switch_duplicate_value_pattern_diagnostic(path, 6, "false");
}

void test_switch_accepts_exhaustive_bool_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_exhaustive_bool_without_default_success.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0"});

    assert_fixture_success(path);
}

void test_switch_rejects_missing_bool_value_pattern_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_missing_bool_pattern_failure.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1"});

    assert_switch_missing_bool_value_pattern_diagnostic(path, 3, "false");
}

void test_switch_rejects_redundant_zero_payload_choice_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_redundant_choice_default_failure.or";
    write_zero_payload_choice_switch_fixture(
        path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3"},
        true
    );

    assert_switch_redundant_zero_payload_choice_default_diagnostic(path, 11);
}

void test_switch_duplicate_zero_payload_choice_suppresses_redundant_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_zero_payload_choice_redundant_default_no_cascade.or";
    write_zero_payload_choice_switch_fixture(
        path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3", "Closed => 4"},
        true
    );

    assert_switch_duplicate_constructor_diagnostic(path, 11, "Closed");
}

void test_switch_accepts_exhaustive_zero_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_exhaustive_choice_without_default_success.or";
    write_zero_payload_choice_switch_fixture(
        path,
        {"Closed => 1", "EndOfInput => 2", "PermissionDenied => 3"}
    );

    assert_fixture_success(path);
}

void test_switch_rejects_redundant_payload_choice_default_after_full_cover_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_redundant_payload_choice_default_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value", "Empty => 0"}, true);

    assert_switch_redundant_choice_default_diagnostic(path, 9);
}

void test_switch_accepts_exhaustive_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_exhaustive_payload_choice_success.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value", "Empty => 0"});

    assert_fixture_success(path);
}

void test_switch_accepts_reversed_exhaustive_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_reversed_exhaustive_payload_choice_success.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Empty => 0", "Some(value) => value"});

    assert_fixture_success(path);
}

void test_switch_accepts_literal_payload_choice_arm_with_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_literal_payload_choice_default_success.or";
    write_maybe_int_exhaustiveness_fixture(path, {"Some(1) => 1", "Empty => 0"}, true);

    assert_fixture_success(path);
}

void test_switch_rejects_literal_payload_choice_arm_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_literal_payload_choice_missing_failure.or";
    write_maybe_int_exhaustiveness_fixture(path, {"Some(1) => 1", "Empty => 0"});

    assert_switch_missing_choice_variant_diagnostic(path, 6, "Some");
}

void test_switch_rejects_reversed_literal_payload_choice_arm_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_reversed_literal_payload_choice_missing_failure.or";
    write_maybe_int_exhaustiveness_fixture(path, {"Empty => 0", "Some(1) => 1"});

    assert_switch_missing_choice_variant_diagnostic(path, 6, "Some");
}

void test_switch_accepts_nested_payload_choice_arm_with_default_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_payload_choice_default_success.or";
    write_boxed_maybe_exhaustiveness_fixture(path, {"Wrap(Some(value)) => value", "Blank => 0"}, true);

    assert_fixture_success(path);
}

void test_switch_rejects_nested_payload_choice_arm_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nested_payload_choice_missing_failure.or";
    write_boxed_maybe_exhaustiveness_fixture(path, {"Wrap(Some(value)) => value", "Blank => 0"});

    assert_switch_missing_choice_variant_diagnostic(path, 9, "Wrap");
}

void test_switch_accepts_partial_multi_payload_choice_arm_with_default_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_partial_multi_payload_choice_default_success.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, 1) => left", "Empty => 0"}, true);

    assert_fixture_success(path);
}

void test_switch_rejects_partial_multi_payload_choice_arm_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_partial_multi_payload_choice_missing_failure.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, 1) => left", "Empty => 0"});

    assert_switch_missing_choice_variant_diagnostic(path, 6, "Both");
}

void test_switch_rejects_missing_payload_choice_variant_without_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_missing_payload_choice_variant_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value"});

    assert_switch_missing_choice_variant_diagnostic(path, 6, "Empty");
}

void test_switch_accepts_exhaustive_multi_payload_choice_without_default_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_exhaustive_multi_payload_choice_success.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        path,
        {"First(value) => value", "Second(value) => value", "Empty => 0"}
    );

    assert_fixture_success(path);
}

void test_switch_rejects_redundant_multi_payload_choice_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_redundant_multi_payload_choice_default_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        path,
        {"First(value) => value", "Second(value) => value", "Empty => 0"},
        true
    );

    assert_switch_redundant_choice_default_diagnostic(path, 11);
}

void test_switch_duplicate_payload_choice_suppresses_redundant_default_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_payload_choice_redundant_default_no_cascade.or";
    write_multi_payload_choice_exhaustiveness_fixture(
        path,
        {"First(value) => value", "Second(value) => value", "Empty => 0", "First(other) => other"},
        true
    );

    assert_switch_duplicate_constructor_diagnostic(path, 11, "First(...)");
}

void test_switch_rejects_first_missing_multi_payload_choice_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_first_missing_multi_payload_choice_variant_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(path, {"Second(value) => value", "Empty => 0"});

    assert_switch_missing_choice_variant_diagnostic(path, 7, "First");
}

void test_switch_rejects_second_missing_multi_payload_choice_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_second_missing_multi_payload_choice_variant_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(path, {"First(value) => value", "Empty => 0"});

    assert_switch_missing_choice_variant_diagnostic(path, 7, "Second");
}

void test_switch_duplicate_multi_payload_choice_without_default_does_not_cascade_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_multi_payload_choice_no_cascade_failure.or";
    write_multi_payload_choice_exhaustiveness_fixture(path, {"First(value) => value", "First(other) => other"});

    assert_switch_duplicate_constructor_diagnostic(path, 9, "First(...)");
}

void test_switch_duplicate_payload_choice_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_payload_choice_without_default_no_cascade_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => value", "Some(other) => other"});

    assert_switch_duplicate_constructor_diagnostic(path, 8, "Some(...)");
}

void test_switch_rejects_duplicate_zero_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_duplicate_choice_constructor_failure.or";
    write_zero_payload_choice_switch_fixture(path, {"Closed => 1", "EndOfInput => 2", "Closed => 3"});

    assert_switch_duplicate_constructor_diagnostic(path, 10, "Closed");
}

void test_switch_duplicate_choice_without_default_does_not_cascade_to_missing_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_choice_without_default_no_cascade_failure.or";
    write_zero_payload_choice_switch_fixture(path, {"Closed => 1", "EndOfInput => 2", "Closed => 3"});

    assert_switch_duplicate_constructor_diagnostic(path, 10, "Closed");
}

void test_switch_rejects_duplicate_name_only_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_name_only_payload_choice_constructor_failure.or";
    write_maybe_choice_exhaustiveness_fixture(path, {"Some(value) => 1", "Some(other) => 2"}, true);

    assert_switch_duplicate_constructor_diagnostic(path, 8, "Some(...)");
}

void test_switch_duplicate_payload_choice_constructor_does_not_cascade_to_binding_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_payload_choice_constructor_no_cascade_failure.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, right) => 1", "Both(value, value) => 2"}, true);

    assert_switch_duplicate_constructor_diagnostic(path, 8, "Both(...)");
}

void test_switch_rejects_duplicate_literal_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_duplicate_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(path, {"Int(1) => 1", "Int(1) => 2"});

    assert_switch_duplicate_constructor_diagnostic(path, 8, "Int(...)");
}

void test_switch_rejects_equivalent_integer_literal_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_equivalent_integer_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(path, {"Int(1) => 1", "Int(1 as Int64) => 2"});

    assert_switch_duplicate_constructor_diagnostic(path, 8, "Int(...)");
}

void test_switch_rejects_wildcard_then_literal_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_wildcard_then_literal_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(path, {"Int(value) => 1", "Int(1) => 2"});

    assert_switch_duplicate_constructor_diagnostic(path, 8, "Int(...)");
}

void test_switch_rejects_literal_then_wildcard_payload_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_literal_then_wildcard_payload_choice_constructor_failure.or";
    write_number_choice_switch_fixture(path, {"Int(1) => 1", "Int(value) => 2"});

    assert_switch_duplicate_constructor_diagnostic(path, 8, "Int(...)");
}

void test_switch_rejects_multi_payload_partial_overlap_choice_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_multi_payload_partial_overlap_choice_constructor_failure.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, 1) => 1", "Both(other, 1) => 2"}, true);

    assert_switch_duplicate_constructor_diagnostic(path, 8, "Both(...)");
}

void test_switch_accepts_multi_payload_disjoint_literal_choice_constructor_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_multi_payload_disjoint_literal_choice_constructor_success.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(left, 1) => 1", "Both(other, 2) => 2"}, true);

    assert_fixture_success(path);
}

void test_switch_accepts_multi_payload_disjoint_leading_literal_choice_constructor_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_switch_multi_payload_disjoint_leading_literal_choice_constructor_success.or";
    write_pair_choice_exhaustiveness_fixture(path, {"Both(1, left) => 1", "Both(2, right) => 2"}, true);

    assert_fixture_success(path);
}

void test_switch_rejects_missing_zero_payload_choice_variant_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_missing_choice_variant_failure.or";
    write_zero_payload_choice_switch_fixture(path, {"Closed => 1", "EndOfInput => 2"});

    assert_switch_missing_zero_payload_choice_variant_diagnostic(path, 7, "PermissionDenied");
}

void test_switch_rejects_multiple_default_cases_semantically() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_multiple_default_semantic_failure.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0"});

    auto diagnostics = analyze_mutated_first_switch_fixture(path, [](auto& switch_statement) {
        switch_statement.switch_cases.front().is_default = true;
        switch_statement.switch_cases.back().is_default = true;
    });
    assert_multiple_switch_default_diagnostics(diagnostics);
}

void test_switch_rejects_nonfinal_default_case_semantically() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_switch_nonfinal_default_semantic_failure.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => 0"});

    auto diagnostics = analyze_mutated_first_switch_fixture(path, [](auto& switch_statement) {
        switch_statement.switch_cases.front().is_default = true;
    });
    assert_nonfinal_switch_default_diagnostic(diagnostics);
}

void test_switch_nonfinal_default_suppresses_branch_analysis_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_nonfinal_default_branch_no_cascade.or";
    write_bool_value_pattern_switch_fixture(path, {"default => await flag", "true => 1"});

    assert_nonfinal_switch_default_diagnostic(path, 4);
}

void test_switch_multiple_default_suppresses_branch_analysis_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_switch_multiple_default_branch_no_cascade.or";
    write_bool_value_pattern_switch_fixture(path, {"true => 1", "false => await flag"});

    auto diagnostics = analyze_mutated_first_switch_fixture(path, [](auto& switch_statement) {
        switch_statement.switch_cases.front().is_default = true;
        switch_statement.switch_cases.back().is_default = true;
    });
    assert_multiple_switch_default_diagnostics(diagnostics);
}

void test_break_outside_loop_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_break_outside_loop_failure.or";
    write_loop_control_fixture(path, "stop() -> Unit", {"break"});

    assert_break_outside_loop_diagnostic(path, 3);
}

void test_continue_outside_loop_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_continue_outside_loop_failure.or";
    write_loop_control_fixture(path, "keep_going() -> Unit", {"continue"});

    assert_continue_outside_loop_diagnostic(path, 3);
}

void test_break_inside_loop_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_break_inside_loop_success.or";
    write_loop_control_fixture(path, "stop(flag: Bool) -> Unit", {"while flag", "    break"});

    assert_fixture_success(path);
}

void test_continue_inside_loop_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_continue_inside_loop_success.or";
    write_loop_control_fixture(
        path,
        "scan(items: shared View<Int64>) -> Unit",
        {"for item in items", "    continue"}
    );

    assert_fixture_success(path);
}

void test_this_outside_method_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_this_outside_method_failure.or";
    write_receiver_fixture(path, {"function current() -> Int64", "    return this"});

    assert_receiver_this_context_diagnostic(path, 3);
}

void test_receiver_parameter_outside_method_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_receiver_parameter_outside_method_failure.or";
    write_receiver_fixture(path, {"function current(this: Int64) -> Int64", "    return 0"});

    assert_receiver_parameter_context_diagnostic(path, 2);
}

void test_qualified_this_type_in_ordinary_function_signature_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_this_type_signature_failure.or";
    write_receiver_fixture(path, {"function project(value: shared This) -> shared This", "    return value"});

    auto diagnostics = analyze_orison_fixture(path);
    assert_this_type_context_diagnostics(diagnostics, 2, 2);
}

void test_qualified_this_type_in_local_annotation_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_this_type_local_failure.or";
    write_receiver_fixture(path, {"function cache() -> Unit", "    let current: exclusive This = unit"});

    assert_fixture_this_type_context_diagnostic(path, 3);
}

void test_receiver_parameter_with_nonself_type_inside_method_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_receiver_parameter_nonself_type_failure.or";
    write_receiver_fixture(path, {"extend Buffer", "    function reset(this: Int64) -> Unit", "        return"});

    assert_receiver_parameter_self_type_diagnostic(path, 3);
}

void test_unsafe_intrinsic_outside_unsafe_context_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unsafe_intrinsic_outside_unsafe_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function read_byte(p: Address) -> Byte",
            "    return raw_read(p)",
        }
    );

    assert_unsafe_intrinsic_context_diagnostic(path, 3, "raw_read");
}

void test_unsafe_intrinsic_inside_unsafe_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unsafe_intrinsic_inside_unsafe_function.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Address) -> Byte",
            "    return raw_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_unsafe_intrinsic_inside_unsafe_block_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unsafe_intrinsic_inside_unsafe_block.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function zero_byte(p: Address) -> Unit",
            "    unsafe",
            "        raw_write(p, 0)",
        }
    );

    assert_fixture_success(path);
}

void test_address_of_nonstorage_operand_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_address_of_nonstorage_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function pointer() -> Address",
            "    return address_of(1)",
        }
    );

    assert_address_of_storage_operand_diagnostic(path, 3);
}

void test_raw_read_nonaddress_operand_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_raw_read_nonaddress_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word() -> UInt32",
            "    return raw_read(1)",
        }
    );

    assert_raw_read_address_operand_diagnostic(path, 3);
}

void test_raw_offset_nonaddress_base_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_raw_offset_nonaddress_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function advance() -> Address",
            "    return raw_offset(1, 2)",
        }
    );

    assert_raw_offset_address_operand_diagnostic(path, 3);
}

void test_raw_offset_noninteger_offset_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_raw_offset_noninteger_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function advance(base: Address) -> Address",
            "    return raw_offset(base, \"one\")",
        }
    );

    assert_raw_offset_integer_offset_diagnostic(path, 3);
}

void test_volatile_read_nonaddress_operand_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_nonaddress_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function uart_ready() -> UInt32",
            "    return volatile_read(1)",
        }
    );

    assert_volatile_read_address_operand_diagnostic(path, 3);
}

void test_address_constant_enables_volatile_read_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_address_constant_volatile_read_success.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const UART_STATUS: Address = 0x4000_1000",
            "unsafe function read_status() -> UInt32",
            "    return volatile_read(UART_STATUS)",
        }
    );

    assert_fixture_success(path);
}

void test_integer_literal_constant_initializer_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_integer_literal_constant_success.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const STATUS_MASK: UInt32 = 0xFF",
            "function mask() -> UInt32",
            "    return STATUS_MASK",
        }
    );

    assert_fixture_success(path);
}

void test_constant_initializer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_constant_initializer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const STATUS_MASK: UInt32 = true",
        }
    );

    assert_constant_initializer_mismatch_diagnostic(path, 2, "Bool", "UInt32");
}

void test_array_literal_constant_initializer_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_success.or";
    write_array_constant_fixture(
        path,
        scalar_array_constant_initializer("1, 2", 2),
        {
            "function first_magic() -> UInt32",
            "    return MAGIC[0]",
        }
    );

    assert_fixture_success(path);
}

void test_array_literal_constant_initializer_element_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_element_failure.or";
    write_array_constant_fixture(path, scalar_array_constant_initializer("true"));

    assert_constant_array_initializer_element_type_diagnostic(path, 2, "Bool", "UInt32");
}

void test_array_literal_constant_initializer_length_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_length_failure.or";
    write_array_constant_fixture(path, scalar_array_constant_initializer("1, 2, 3", 2));

    assert_constant_array_initializer_length_diagnostic(path, 2, 3, 2);
}

void test_nested_array_literal_constant_initializer_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_nested_array_literal_constant_success.or";
    write_nested_array_constant_fixture(
        path,
        nested_array_constant_initializer("[1, 2], [3, 4]", 2),
        {
            "function first_value() -> UInt32",
            "    return MATRIX[0][0]",
        }
    );

    assert_fixture_success(path);
}

void test_indexed_array_constant_initializer_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_indexed_array_constant_success.or";
    write_array_constant_fixture(
        path,
        scalar_array_constant_initializer("1, 2", 2),
        {
            "const FIRST_MAGIC: UInt32 = MAGIC[0]",
            "function first_magic() -> UInt32",
            "    return FIRST_MAGIC",
        }
    );

    assert_fixture_success(path);
}

void test_indexed_array_constant_initializer_element_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_indexed_array_constant_element_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const FLAGS: Array<Bool, 1> = [true]",
            "const FIRST_FLAG: UInt32 = FLAGS[0]",
        }
    );

    assert_constant_initializer_mismatch_diagnostic(path, 3, "Bool", "UInt32");
}

void test_record_constructor_constant_initializer_field_access_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_record_constructor_constant_field_success.or";
    write_header_record_constant_fixture(
        path,
        {
            "const DEFAULT_HEADER: Header = Header([1, 2], 1)",
            "const DEFAULT_VERSION: UInt16 = DEFAULT_HEADER.version",
            "function default_version() -> UInt16",
            "    return DEFAULT_VERSION",
        }
    );

    assert_fixture_success(path);
}

void test_record_constructor_constant_initializer_indexed_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_record_constructor_constant_indexed_field_success.or";
    write_header_record_constant_fixture(
        path,
        {
            "const DEFAULT_MAGIC: UInt32 = Header([1, 2], 1).magic[0]",
            "function default_magic() -> UInt32",
            "    return DEFAULT_MAGIC",
        }
    );

    assert_fixture_success(path);
}

void test_record_constructor_constant_initializer_arity_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_record_constructor_constant_arity_failure.or";
    write_header_record_constant_fixture(path, {"const DEFAULT_HEADER: Header = Header([1, 2])"});

    assert_record_constructor_arity_diagnostic(path, 5, "Header", 2, 1);
}

void test_record_constructor_constant_initializer_field_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_record_constructor_constant_field_type_failure.or";
    write_header_record_constant_fixture(path, {"const DEFAULT_HEADER: Header = Header([1, 2], true)"});

    assert_record_constructor_field_type_diagnostic(path, 5, "version", "Bool", "UInt16");
}

void test_record_constructor_let_binding_field_access_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_record_constructor_let_field_success.or";
    write_header_record_constant_fixture(
        path,
        {
            "function default_version() -> UInt16",
            "    let header = Header([1, 2], 1)",
            "    return header.version",
        }
    );

    assert_fixture_success(path);
}

void test_record_constructor_let_binding_field_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_record_constructor_let_field_type_failure.or";
    write_header_record_constant_fixture(
        path,
        {
            "function default_version() -> UInt16",
            "    let header = Header([1, 2], true)",
            "    return 0",
        }
    );

    assert_record_constructor_field_type_diagnostic(path, 6, "version", "Bool", "UInt16");
}

void test_record_constructor_return_expression_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_record_constructor_return_success.or";
    write_header_record_constant_fixture(
        path,
        {
            "function default_header() -> Header",
            "    return Header([1, 2], 1)",
        }
    );

    assert_fixture_success(path);
}

void test_record_constructor_return_expression_arity_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_record_constructor_return_arity_failure.or";
    write_header_record_constant_fixture(
        path,
        {
            "function default_header() -> Header",
            "    return Header([1, 2])",
        }
    );

    assert_record_constructor_arity_diagnostic(path, 6, "Header", 2, 1);
}

void test_generic_record_constructor_repeated_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_record_repeated_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Same<T>",
            "    first: T",
            "    second: T",
            "function demo() -> UInt16",
            "    let same = Same(1 as UInt16, 2 as Int16)",
            "    return same.first",
        }
    );

    assert_fixture_success(path);
}

void test_generic_record_constructor_repeated_field_conflict_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_record_repeated_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record Same<T>",
            "    first: T",
            "    second: T",
            "function demo() -> UInt16",
            "    let same = Same(Header([1, 2], 1), OtherHeader([1, 2], 1))",
            "    return same.first.version",
        }
    );

    assert_record_constructor_field_type_diagnostic(path, 12, "second", "OtherHeader", "Header");
}

void test_record_constructor_choice_ternary_field_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_record_choice_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_record_fixture_lines("    let box: Box<UInt32> = Box(flag ? Some(true) : Empty)", false)
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 8, "Bool", "UInt32");
}

void test_record_constructor_choice_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_record_choice_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_record_fixture_lines("    let box: Box<UInt32> = Box(flag ? Some(1 as UInt32) : Empty)", false)
    );

    assert_fixture_success(path);
}

void test_record_constructor_pointer_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_record_pointer_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        slot_pointer_record_fixture_lines(
            "    let slot: Slot<UInt32> = Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
            false
        )
    );

    assert_raw_offset_source_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_record_constructor_pointer_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_record_pointer_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        slot_pointer_record_success_fixture_lines(
            "    let slot: Slot<UInt32> = Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
            false
        )
    );

    assert_fixture_success(path);
}

void test_nested_record_constructor_choice_ternary_field_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_record_choice_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_record_fixture_lines(
            "    let outer: Outer<UInt32> = Outer(Box(flag ? Some(true) : Empty))",
            true
        )
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 10, "Bool", "UInt32");
}

void test_nested_record_constructor_choice_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_record_choice_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_record_fixture_lines(
            "    let outer: Outer<UInt32> = Outer(Box(flag ? Some(1 as UInt32) : Empty))",
            true
        )
    );

    assert_fixture_success(path);
}

void test_nested_record_constructor_pointer_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_record_pointer_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        slot_pointer_record_fixture_lines(
            "    let wrapper: Wrapper<UInt32> = Wrapper(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            true
        )
    );

    assert_raw_offset_source_pointee_mismatch_diagnostic(path, 7, "Byte", "UInt32");
}

void test_nested_record_constructor_pointer_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_record_pointer_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        slot_pointer_record_success_fixture_lines(
            "    let wrapper: Wrapper<UInt32> = Wrapper(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            true
        )
    );

    assert_fixture_success(path);
}

void test_choice_payload_record_constructor_choice_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_payload_record_choice_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Item(value: Box<T>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Box(flag ? Some(true) : Empty))",
            "    return 1",
        }
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 10, "Bool", "UInt32");
}

void test_choice_payload_record_constructor_choice_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_payload_record_choice_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Item(value: Box<T>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Box(flag ? Some(1 as UInt32) : Empty))",
            "    return 1",
        }
    );

    assert_fixture_success(path);
}

void test_array_record_constructor_choice_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_record_choice_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let boxes: Array<Box<UInt32>, 1> = [Box(flag ? Some(true) : Empty)]",
            "    return 1",
        }
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 8, "Bool", "UInt32");
}

void test_array_record_constructor_choice_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_record_choice_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let boxes: Array<Box<UInt32>, 1> = [Box(flag ? Some(1 as UInt32) : Empty)]",
            "    return 1",
        }
    );

    assert_fixture_success(path);
}

void test_choice_payload_record_constructor_pointer_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_payload_record_pointer_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Item(value: Slot<T>)",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            "    return 1",
        }
    );

    assert_raw_offset_source_pointee_mismatch_diagnostic(path, 7, "Byte", "UInt32");
}

void test_choice_payload_record_constructor_pointer_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_payload_record_pointer_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Item(value: Slot<T>)",
            "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            "    return 1",
        }
    );

    assert_fixture_success(path);
}

void test_array_record_constructor_pointer_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_record_pointer_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    let slots: Array<Slot<UInt32>, 1> = [Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]",
            "    return 1",
        }
    );

    assert_raw_offset_source_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_array_record_constructor_pointer_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_record_pointer_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32",
            "    let slots: Array<Slot<UInt32>, 1> = [Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]",
            "    return 1",
        }
    );

    assert_fixture_success(path);
}

void test_nested_array_record_constructor_choice_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_array_record_choice_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_record_fixture_lines(
            "    let boxes: Array<Array<Box<UInt32>, 1>, 1> = [[Box(flag ? Some(true) : Empty)]]",
            false
        )
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 8, "Bool", "UInt32");
}

void test_nested_array_record_constructor_choice_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_array_record_choice_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_record_fixture_lines(
            "    let boxes: Array<Array<Box<UInt32>, 1>, 1> = [[Box(flag ? Some(1 as UInt32) : Empty)]]",
            false
        )
    );

    assert_fixture_success(path);
}

void test_nested_array_record_constructor_pointer_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_array_record_pointer_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        slot_pointer_record_fixture_lines(
            "    let slots: Array<Array<Slot<UInt32>, 1>, 1> = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]",
            false
        )
    );

    assert_raw_offset_source_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_nested_array_record_constructor_pointer_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_array_record_pointer_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        slot_pointer_record_success_fixture_lines(
            "    let slots: Array<Array<Slot<UInt32>, 1>, 1> = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]",
            false
        )
    );

    assert_fixture_success(path);
}

void test_record_field_nested_array_record_constructor_choice_ternary_field_failure() {
    assert_record_field_nested_array_choice_context_failure(
        "orison_semantics_record_field_nested_array_record_choice_ternary_field_failure.or",
        box_maybe_nested_array_field_fixture_lines(
            "    let shelf: Shelf<UInt32> = Shelf([[Box(flag ? Some(true) : Empty)]])"
        ),
        10
    );
}

void test_record_field_nested_array_record_constructor_choice_ternary_field_success() {
    assert_record_field_nested_array_context_success(
        "orison_semantics_record_field_nested_array_record_choice_ternary_field_success.or",
        box_maybe_nested_array_field_fixture_lines(
            "    let shelf: Shelf<UInt32> = Shelf([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
}

void test_record_field_nested_array_record_constructor_pointer_ternary_field_failure() {
    assert_record_field_nested_array_pointer_context_failure(
        "orison_semantics_record_field_nested_array_record_pointer_ternary_field_failure.or",
        slot_pointer_nested_array_field_fixture_lines(
            "    let rack: Rack<UInt32> = Rack([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        ),
        7
    );
}

void test_record_field_nested_array_record_constructor_pointer_ternary_field_success() {
    assert_record_field_nested_array_context_success(
        "orison_semantics_record_field_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_nested_array_field_success_fixture_lines(
            "    let rack: Rack<UInt32> = Rack([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
}

void test_record_field_nested_array_choice_constructor_ternary_payload_failure() {
    assert_record_field_nested_array_choice_context_failure(
        "orison_semantics_record_field_nested_array_choice_constructor_ternary_payload_failure.or",
        maybe_nested_array_field_fixture_lines(
            "    let shelf: ChoiceShelf<UInt32> = ChoiceShelf([[flag ? Some(true) : Empty]])"
        ),
        8
    );
}

void test_record_field_nested_array_choice_constructor_ternary_payload_success() {
    assert_record_field_nested_array_context_success(
        "orison_semantics_record_field_nested_array_choice_constructor_ternary_payload_success.or",
        maybe_nested_array_field_fixture_lines(
            "    let shelf: ChoiceShelf<UInt32> = ChoiceShelf([[flag ? Some(1 as UInt32) : Empty]])"
        )
    );
}

void test_call_argument_nested_array_record_constructor_choice_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_call_argument_nested_array_record_choice_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_nested_array_argument_fixture_lines(
            "    return consume([[Box(flag ? Some(true) : Empty)]])"
        )
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 10, "Bool", "UInt32");
}

void test_call_argument_nested_array_record_constructor_choice_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_call_argument_nested_array_record_choice_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_nested_array_argument_fixture_lines(
            "    return consume([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );

    assert_fixture_success(path);
}

void test_call_argument_nested_array_choice_constructor_ternary_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_call_argument_nested_array_choice_constructor_ternary_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        maybe_nested_array_argument_fixture_lines(
            "    return consume([[flag ? Some(true) : Empty]])"
        )
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 8, "Bool", "UInt32");
}

void test_call_argument_nested_array_choice_constructor_ternary_payload_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_call_argument_nested_array_choice_constructor_ternary_payload_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        maybe_nested_array_argument_fixture_lines(
            "    return consume([[flag ? Some(1 as UInt32) : Empty]])"
        )
    );

    assert_fixture_success(path);
}

void test_method_argument_nested_array_record_constructor_choice_ternary_field_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_method_argument_nested_array_record_choice_ternary_field_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_nested_array_method_argument_fixture_lines(
            "    return consumer.consume([[Box(flag ? Some(true) : Empty)]])"
        )
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 13, "Bool", "UInt32");
}

void test_method_argument_nested_array_record_constructor_choice_ternary_field_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_method_argument_nested_array_record_choice_ternary_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_nested_array_method_argument_fixture_lines(
            "    return consumer.consume([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );

    assert_fixture_success(path);
}

void test_method_argument_nested_array_choice_constructor_ternary_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_method_argument_nested_array_choice_constructor_ternary_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        maybe_nested_array_method_argument_fixture_lines(
            "    return consumer.consume([[flag ? Some(true) : Empty]])"
        )
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 11, "Bool", "UInt32");
}

void test_method_argument_nested_array_choice_constructor_ternary_payload_success() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_method_argument_nested_array_choice_constructor_ternary_payload_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        maybe_nested_array_method_argument_fixture_lines(
            "    return consumer.consume([[flag ? Some(1 as UInt32) : Empty]])"
        )
    );

    assert_fixture_success(path);
}

void assert_choice_payload_items_choice_ternary_failure(
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    int diagnostic_line,
    std::string_view payload_type = "Array<Box<T>, 1>"
) {
    auto path = std::filesystem::temp_directory_path() / std::string(fixture_name);
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_items_fixture_lines(binding_line, include_holder, payload_type)
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, diagnostic_line, "Bool", "UInt32");
}

void assert_choice_payload_items_choice_ternary_success(
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Box<T>, 1>"
) {
    auto path = std::filesystem::temp_directory_path() / std::string(fixture_name);
    write_concurrency_fixture(
        path,
        "demo.records",
        box_maybe_items_fixture_lines(binding_line, include_holder, payload_type)
    );

    assert_fixture_success(path);
}

void assert_choice_payload_items_pointer_ternary_failure(
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    int diagnostic_line,
    std::string_view payload_type = "Array<Slot<T>, 1>"
) {
    auto path = std::filesystem::temp_directory_path() / std::string(fixture_name);
    write_concurrency_fixture(
        path,
        "demo.records",
        slot_pointer_items_fixture_lines(binding_line, include_holder, payload_type)
    );

    assert_raw_offset_source_pointee_mismatch_diagnostic(path, diagnostic_line, "Byte", "UInt32");
}

void assert_choice_payload_items_pointer_ternary_success(
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
) {
    auto path = std::filesystem::temp_directory_path() / std::string(fixture_name);
    write_concurrency_fixture(
        path,
        "demo.records",
        slot_pointer_items_success_fixture_lines(binding_line, include_holder, payload_type)
    );

    assert_fixture_success(path);
}

void test_choice_payload_array_record_constructor_choice_ternary_field_failure() {
    assert_choice_payload_items_choice_ternary_failure(
        "orison_semantics_choice_payload_array_record_choice_ternary_field_failure.or",
        "    let item: Wrap<UInt32> = Items([Box(flag ? Some(true) : Empty)])",
        false,
        10
    );
}

void test_choice_payload_array_record_constructor_choice_ternary_field_success() {
    assert_choice_payload_items_choice_ternary_success(
        "orison_semantics_choice_payload_array_record_choice_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([Box(flag ? Some(1 as UInt32) : Empty)])",
        false
    );
}

void test_choice_payload_array_record_constructor_pointer_ternary_field_failure() {
    assert_choice_payload_items_pointer_ternary_failure(
        "orison_semantics_choice_payload_array_record_pointer_ternary_field_failure.or",
        "    let item: Wrap<UInt32> = Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))])",
        false,
        7
    );
}

void test_choice_payload_array_record_constructor_pointer_ternary_field_success() {
    assert_choice_payload_items_pointer_ternary_success(
        "orison_semantics_choice_payload_array_record_pointer_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))])",
        false
    );
}

void test_choice_payload_nested_array_record_constructor_choice_ternary_field_failure() {
    assert_choice_payload_items_choice_ternary_failure(
        "orison_semantics_choice_payload_nested_array_record_choice_ternary_field_failure.or",
        "    let item: Wrap<UInt32> = Items([[Box(flag ? Some(true) : Empty)]])",
        false,
        10,
        "Array<Array<Box<T>, 1>, 1>"
    );
}

void test_choice_payload_nested_array_record_constructor_choice_ternary_field_success() {
    assert_choice_payload_items_choice_ternary_success(
        "orison_semantics_choice_payload_nested_array_record_choice_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([[Box(flag ? Some(1 as UInt32) : Empty)]])",
        false,
        "Array<Array<Box<T>, 1>, 1>"
    );
}

void test_choice_payload_nested_array_record_constructor_pointer_ternary_field_failure() {
    assert_choice_payload_items_pointer_ternary_failure(
        "orison_semantics_choice_payload_nested_array_record_pointer_ternary_field_failure.or",
        "    let item: Wrap<UInt32> = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])",
        false,
        7,
        "Array<Array<Slot<T>, 1>, 1>"
    );
}

void test_choice_payload_nested_array_record_constructor_pointer_ternary_field_success() {
    assert_choice_payload_items_pointer_ternary_success(
        "orison_semantics_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])",
        false,
        "Array<Array<Slot<T>, 1>, 1>"
    );
}

void test_record_field_choice_payload_array_record_constructor_choice_ternary_field_failure() {
    assert_choice_payload_items_choice_ternary_failure(
        "orison_semantics_record_field_choice_payload_array_record_choice_ternary_field_failure.or",
        "    let holder: Holder<UInt32> = Holder(Items([Box(flag ? Some(true) : Empty)]))",
        true,
        12
    );
}

void test_record_field_choice_payload_array_record_constructor_choice_ternary_field_success() {
    assert_choice_payload_items_choice_ternary_success(
        "orison_semantics_record_field_choice_payload_array_record_choice_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([Box(flag ? Some(1 as UInt32) : Empty)]))",
        true
    );
}

void test_record_field_choice_payload_array_record_constructor_pointer_ternary_field_failure() {
    assert_choice_payload_items_pointer_ternary_failure(
        "orison_semantics_record_field_choice_payload_array_record_pointer_ternary_field_failure.or",
        "    let holder: Holder<UInt32> = Holder(Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]))",
        true,
        9
    );
}

void test_record_field_choice_payload_array_record_constructor_pointer_ternary_field_success() {
    assert_choice_payload_items_pointer_ternary_success(
        "orison_semantics_record_field_choice_payload_array_record_pointer_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]))",
        true
    );
}

void test_record_field_choice_payload_nested_array_record_constructor_choice_ternary_field_failure() {
    assert_choice_payload_items_choice_ternary_failure(
        "orison_semantics_record_field_choice_payload_nested_array_record_choice_ternary_field_failure.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Box(flag ? Some(true) : Empty)]]))",
        true,
        12,
        "Array<Array<Box<T>, 1>, 1>"
    );
}

void test_record_field_choice_payload_nested_array_record_constructor_choice_ternary_field_success() {
    assert_choice_payload_items_choice_ternary_success(
        "orison_semantics_record_field_choice_payload_nested_array_record_choice_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))",
        true,
        "Array<Array<Box<T>, 1>, 1>"
    );
}

void test_record_field_choice_payload_nested_array_record_constructor_pointer_ternary_field_failure() {
    assert_choice_payload_items_pointer_ternary_failure(
        "orison_semantics_record_field_choice_payload_nested_array_record_pointer_ternary_field_failure.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))",
        true,
        9,
        "Array<Array<Slot<T>, 1>, 1>"
    );
}

void test_record_field_choice_payload_nested_array_record_constructor_pointer_ternary_field_success() {
    assert_choice_payload_items_pointer_ternary_success(
        "orison_semantics_record_field_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))",
        true,
        "Array<Array<Slot<T>, 1>, 1>"
    );
}

void test_annotated_record_binding_constructor_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_annotated_record_binding_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "function default_header() -> Header",
            "    let header: OtherHeader = Header([1, 2], 1)",
            "    return Header([1, 2], 1)",
        }
    );

    assert_binding_initializer_type_mismatch_diagnostic(path, 9, "Header", "OtherHeader");
}

void test_record_return_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_record_return_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "function default_header() -> OtherHeader",
            "    return Header([1, 2], 1)",
        }
    );

    assert_return_expression_type_mismatch_diagnostic(path, 9, "Header", "OtherHeader");
}

void test_annotated_integer_binding_same_width_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_annotated_integer_binding_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "function status() -> UInt32",
            "    let value: UInt32 = 1 as Int32",
            "    return value",
        }
    );

    assert_fixture_success(path);
}

void test_record_assignment_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_record_assignment_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "function default_header() -> Header",
            "    var header = Header([1, 2], 1)",
            "    header = OtherHeader([1, 2], 1)",
            "    return header",
        }
    );

    assert_assignment_value_type_mismatch_diagnostic(path, 10, "OtherHeader", "Header");
}

void test_record_field_assignment_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_record_field_assignment_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record Wrapper",
            "    header: Header",
            "function default_header(wrapper: Wrapper) -> Header",
            "    wrapper.header = OtherHeader([1, 2], 1)",
            "    return wrapper.header",
        }
    );

    assert_assignment_value_type_mismatch_diagnostic(path, 11, "OtherHeader", "Header");
}

void test_function_call_record_argument_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_function_call_record_argument_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "function consume_header(header: Header) -> UInt16",
            "    return header.version",
            "function demo() -> UInt16",
            "    return consume_header(OtherHeader([1, 2], 1))",
        }
    );

    assert_function_argument_type_mismatch_diagnostic(path, 11, "header", "OtherHeader", "Header");
}

void test_method_call_record_argument_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_method_call_record_argument_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record Processor",
            "    id: UInt32",
            "extend Processor",
            "    function consume(this: shared This, header: Header) -> UInt16",
            "        return header.version",
            "function demo(processor: Processor) -> UInt16",
            "    return processor.consume(OtherHeader([1, 2], 1))",
        }
    );

    assert_method_argument_type_mismatch_diagnostic(path, 14, "header", "OtherHeader", "Header");
}

void test_generic_function_dependent_argument_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_function_dependent_argument_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record Pair<A, B>",
            "    first: A",
            "    second: B",
            "function consume_pair<T>(left: T, pair: Pair<T, UInt16>) -> UInt16",
            "    return pair.second",
            "function demo() -> UInt16",
            "    return consume_pair(Header([1, 2], 1), Pair(Header([1, 2], 1), 1 as UInt16))",
        }
    );

    assert_fixture_success(path);
}

void test_generic_function_dependent_same_width_integer_argument_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_generic_function_dependent_same_width_integer_argument_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record Pair<A, B>",
            "    first: A",
            "    second: B",
            "function consume_pair<T>(left: T, pair: Pair<T, UInt16>) -> UInt16",
            "    return pair.second",
            "function demo() -> UInt16",
            "    return consume_pair(Header([1, 2], 1), Pair(Header([1, 2], 1), 1 as Int16))",
        }
    );

    assert_fixture_success(path);
}

void test_generic_function_dependent_argument_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_function_dependent_argument_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record Pair<A, B>",
            "    first: A",
            "    second: B",
            "function consume_pair<T>(left: T, pair: Pair<T, UInt16>) -> UInt16",
            "    return pair.second",
            "function demo() -> UInt16",
            "    return consume_pair(Header([1, 2], 1), Pair(OtherHeader([1, 2], 1), 1 as UInt16))",
        }
    );

    assert_function_argument_type_mismatch_diagnostic(
        path,
        14,
        "pair",
        "Pair<OtherHeader, UInt16>",
        "Pair<Header, UInt16>"
    );
}

void test_generic_function_repeated_binding_conflict_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_function_repeated_binding_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "function same<T>(left: T, right: T) -> UInt16",
            "    return 1",
            "function demo() -> UInt16",
            "    return same(Header([1, 2], 1), OtherHeader([1, 2], 1))",
        }
    );

    assert_function_argument_type_mismatch_diagnostic(path, 11, "right", "OtherHeader", "Header");
}

void test_generic_method_dependent_same_width_integer_argument_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_generic_method_dependent_same_width_integer_argument_success.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record Pair<A, B>",
            "    first: A",
            "    second: B",
            "record Box<T>",
            "    value: T",
            "extend Box<T>",
            "    function consume_pair(this: shared This, pair: Pair<T, UInt16>) -> UInt16",
            "        return pair.second",
            "function demo(box: Box<Header>) -> UInt16",
            "    return box.consume_pair(Pair(Header([1, 2], 1), 1 as Int16))",
        }
    );

    assert_fixture_success(path);
}

void test_generic_method_repeated_binding_conflict_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_method_repeated_binding_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record Processor",
            "    id: UInt32",
            "extend Processor",
            "    function same<T>(this: shared This, left: T, right: T) -> UInt16",
            "        return 1",
            "function demo(processor: Processor) -> UInt16",
            "    return processor.same(Header([1, 2], 1), OtherHeader([1, 2], 1))",
        }
    );

    assert_method_argument_type_mismatch_diagnostic(path, 14, "right", "OtherHeader", "Header");
}

void test_generic_method_dependent_argument_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_method_dependent_argument_failure.or";
    write_concurrency_fixture(
        path,
        "demo.records",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record Pair<A, B>",
            "    first: A",
            "    second: B",
            "record Box<T>",
            "    value: T",
            "extend Box<T>",
            "    function consume_pair(this: shared This, pair: Pair<T, UInt16>) -> UInt16",
            "        return pair.second",
            "function demo(box: Box<Header>) -> UInt16",
            "    return box.consume_pair(Pair(OtherHeader([1, 2], 1), 1 as UInt16))",
        }
    );

    assert_method_argument_type_mismatch_diagnostic(
        path,
        17,
        "pair",
        "Pair<OtherHeader, UInt16>",
        "Pair<Header, UInt16>"
    );
}

void test_nested_array_literal_constant_initializer_element_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_array_literal_constant_element_failure.or";
    write_nested_array_constant_fixture(path, nested_array_constant_initializer("[1, true]"));

    assert_constant_array_initializer_element_type_diagnostic(path, 2, "Bool", "UInt32");
}

void test_nested_array_literal_constant_initializer_length_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_array_literal_constant_length_failure.or";
    write_nested_array_constant_fixture(path, nested_array_constant_initializer("[1, 2, 3]"));

    assert_constant_array_initializer_length_diagnostic(path, 2, 3, 2);
}

void test_array_literal_constant_initializer_forward_reference_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_forward_reference_success.or";
    write_array_constant_fixture(
        path,
        scalar_array_constant_initializer("STATUS_LOW, STATUS_HIGH", 2),
        {
            "const STATUS_LOW: UInt32 = 1",
            "const STATUS_HIGH: UInt32 = 2",
            "function first_magic() -> UInt32",
            "    return MAGIC[0]",
        }
    );

    assert_fixture_success(path);
}

void test_array_literal_constant_initializer_unknown_reference_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_unknown_reference_failure.or";
    write_array_constant_fixture(path, scalar_array_constant_initializer("STATUS_LOW"));

    assert_constant_initializer_unknown_name_diagnostic(path, 2, "STATUS_LOW");
}

void test_array_literal_constant_initializer_direct_cycle_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_direct_cycle_failure.or";
    write_array_constant_fixture(path, scalar_array_constant_initializer("MAGIC[0]"));

    assert_constant_initializer_cycle_diagnostic(path, 2, "MAGIC");
}

void test_array_literal_constant_initializer_indirect_cycle_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_indirect_cycle_failure.or";
    write_array_constant_fixture(
        path,
        scalar_array_constant_initializer("STATUS_LOW"),
        {
            "const STATUS_LOW: UInt32 = MAGIC[0]",
        }
    );

    assert_constant_initializer_cycle_diagnostic(path, 3, "MAGIC");
}

void test_address_array_literal_constant_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_array_literal_constant_success.or";
    write_array_constant_fixture(
        path,
        "const UART_REGISTERS: Array<Address, 2> = [UART0_DATA, UART0_STATUS]",
        {
            "const UART0_DATA: Address = 0x4000_1000",
            "const UART0_STATUS: Address = 0x4000_1004",
            "function first_register() -> Address",
            "    return UART_REGISTERS[0]",
        }
    );

    assert_fixture_success(path);
}

void test_address_array_literal_constant_initializer_element_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_array_literal_constant_element_failure.or";
    write_array_constant_fixture(path, "const UART_REGISTERS: Array<Address, 1> = [\"uart\"]");

    assert_constant_array_initializer_element_type_diagnostic(path, 2, "Text", "Address");
}

void test_pointer_array_literal_constant_initializer_pointer_construction_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_pointer_array_literal_constant_pointer_construction_failure.or";
    write_array_constant_fixture(
        path,
        "const UART_POINTERS: Array<Pointer<UInt32>, 1> = [Pointer(UART0_DATA)]",
        {
            "const UART0_DATA: Address = 0x4000_1000",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 2, "Pointer construction");
}

void test_choice_array_literal_constant_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_array_literal_constant_success.or";
    write_maybe_array_choice_constant_fixture(
        path,
        "const DEFAULT_VALUES: Array<Maybe<UInt32>, 2> = [Some(0xFF), Empty]"
    );

    assert_fixture_success(path);
}

void test_choice_array_literal_constant_initializer_payload_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_array_literal_constant_payload_failure.or";
    write_maybe_array_choice_constant_fixture(
        path,
        "const DEFAULT_VALUES: Array<Maybe<UInt32>, 1> = [Some(true)]"
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 5, "Bool", "UInt32");
}

void test_nested_choice_array_literal_constant_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_choice_array_literal_constant_success.or";
    write_boxed_maybe_array_choice_constant_fixture(
        path,
        "const DEFAULT_VALUES: Array<Boxed<Maybe<UInt32>>, 1> = [Wrap(Some(0xFF))]"
    );

    assert_fixture_success(path);
}

void test_nested_choice_array_literal_constant_initializer_payload_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_nested_choice_array_literal_constant_payload_failure.or";
    write_boxed_maybe_array_choice_constant_fixture(
        path,
        "const DEFAULT_VALUES: Array<Boxed<Maybe<UInt32>>, 1> = [Wrap(Some(true))]"
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 7, "Bool", "UInt32");
}

void test_array_payload_choice_constant_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_constant_array_payload_success.or";
    write_maybe_choice_constant_fixture(
        path,
        "const DEFAULT_VALUE: Maybe<Array<UInt32, 2>> = Some([1, 2])"
    );

    assert_fixture_success(path);
}

void test_array_payload_choice_constant_initializer_length_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_constant_array_payload_length_failure.or";
    write_maybe_choice_constant_fixture(
        path,
        "const DEFAULT_VALUE: Maybe<Array<UInt32, 2>> = Some([1, 2, 3])"
    );

    assert_constant_array_initializer_length_diagnostic(path, 5, 3, 2);
}

void test_array_payload_choice_constant_initializer_element_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_constant_array_payload_element_failure.or";
    write_maybe_choice_constant_fixture(
        path,
        "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([true])"
    );

    assert_constant_array_initializer_element_type_diagnostic(path, 5, "Bool", "UInt32");
}

void test_array_literal_constant_initializer_function_call_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_function_call_failure.or";
    write_scalar_array_runtime_constant_fixture(
        path,
        "mask(0xFFFF)",
        {
            "function mask(value: UInt32) -> UInt32",
            "    return value bit_and 0xFF",
        }
    );

    assert_constant_initializer_function_call_diagnostic(path, 2, "mask");
}

void test_array_literal_constant_initializer_method_call_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_method_call_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "extend Int64",
            "    function low_byte(this: shared This) -> UInt32",
            "        return 0xFF as UInt32",
            "const STATUS_WORD: Int64 = 0xFFFF",
            "const MAGIC: Array<UInt32, 1> = [STATUS_WORD.low_byte()]",
        }
    );

    assert_constant_initializer_method_call_diagnostic(path, 6, "low_byte");
}

void test_array_payload_choice_constant_initializer_function_call_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_choice_constant_array_payload_function_call_failure.or";
    write_maybe_array_payload_runtime_constant_fixture(
        path,
        "mask(0xFFFF)",
        {
            "function mask(value: UInt32) -> UInt32",
            "    return value bit_and 0xFF",
        }
    );

    assert_constant_initializer_function_call_diagnostic(path, 5, "mask");
}

void test_array_payload_choice_constant_initializer_method_call_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_choice_constant_array_payload_method_call_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "extend Int64",
            "    function low_byte(this: shared This) -> UInt32",
            "        return 0xFF as UInt32",
            "const STATUS_WORD: Int64 = 0xFFFF",
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([STATUS_WORD.low_byte()])",
        }
    );

    assert_constant_initializer_method_call_diagnostic(path, 9, "low_byte");
}

void test_array_literal_constant_initializer_unsafe_intrinsic_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_unsafe_intrinsic_failure.or";
    write_scalar_array_runtime_constant_fixture(
        path,
        "raw_read(UART_STATUS)",
        {
            "const UART_STATUS: Address = 0x4000_1000",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 2, "unsafe intrinsic 'raw_read'");
}

void test_array_payload_choice_constant_initializer_unsafe_intrinsic_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_choice_constant_array_payload_unsafe_intrinsic_failure.or";
    write_maybe_array_payload_runtime_constant_fixture(
        path,
        "raw_read(UART_STATUS)",
        {
            "const UART_STATUS: Address = 0x4000_1000",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 5, "unsafe intrinsic 'raw_read'");
}

void test_array_literal_constant_initializer_await_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_await_failure.or";
    write_scalar_array_runtime_constant_fixture(
        path,
        "await STATUS_MASK",
        {
            "const STATUS_MASK: UInt32 = 0xFF",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 2, "await expression");
}

void test_array_payload_choice_constant_initializer_await_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_choice_constant_array_payload_await_failure.or";
    write_maybe_array_payload_runtime_constant_fixture(
        path,
        "await STATUS_MASK",
        {
            "const STATUS_MASK: UInt32 = 0xFF",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 5, "await expression");
}

void test_array_literal_constant_initializer_task_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_task_failure.or";
    write_scalar_array_block_runtime_constant_fixture(path, "task");

    assert_constant_initializer_runtime_construct_diagnostic(path, 2, "task expression");
}

void test_array_payload_choice_constant_initializer_task_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_choice_constant_array_payload_task_failure.or";
    write_maybe_array_payload_block_runtime_constant_fixture(path, "task");

    assert_constant_initializer_runtime_construct_diagnostic(path, 5, "task expression");
}

void test_array_literal_constant_initializer_thread_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_array_literal_constant_thread_failure.or";
    write_scalar_array_block_runtime_constant_fixture(path, "thread");

    assert_constant_initializer_runtime_construct_diagnostic(path, 2, "thread expression");
}

void test_array_payload_choice_constant_initializer_thread_failure() {
    auto path =
        std::filesystem::temp_directory_path() /
        "orison_semantics_choice_constant_array_payload_thread_failure.or";
    write_maybe_array_payload_block_runtime_constant_fixture(path, "thread");

    assert_constant_initializer_runtime_construct_diagnostic(path, 5, "thread expression");
}

void test_address_constant_initializer_structural_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_address_constant_structural_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const UART_STATUS: Address = \"uart\"",
        }
    );

    assert_address_constant_initializer_diagnostic(path, 2);
}

void test_pointer_constant_initializer_structural_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_constant_structural_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const UART_STATUS: Pointer<UInt32> = 1",
        }
    );

    assert_pointer_typed_constant_initializer_diagnostic(path, 2);
}

void test_forward_constant_initializer_reference_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_forward_constant_reference_success.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const MASKED: UInt32 = STATUS_MASK",
            "const STATUS_MASK: UInt32 = 0xFF",
            "function mask() -> UInt32",
            "    return MASKED",
        }
    );

    assert_fixture_success(path);
}

void test_unknown_constant_initializer_reference_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unknown_constant_reference_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const MASKED: UInt32 = STATUS_MASK",
        }
    );

    assert_constant_initializer_unknown_name_diagnostic(path, 2, "STATUS_MASK");
}

void test_duplicate_top_level_constant_name_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_duplicate_constant_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const STATUS_MASK: UInt32 = 0xFF",
            "const STATUS_MASK: UInt32 = 0x0F",
        }
    );

    assert_duplicate_top_level_constant_diagnostic(path, 3, "STATUS_MASK");
}

void test_direct_constant_initializer_cycle_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_direct_constant_cycle_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const STATUS_MASK: UInt32 = STATUS_MASK",
        }
    );

    assert_constant_initializer_cycle_diagnostic(path, 2, "STATUS_MASK");
}

void test_indirect_constant_initializer_cycle_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_indirect_constant_cycle_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const STATUS_MASK: UInt32 = MASKED",
            "const MASKED: UInt32 = STATUS_MASK",
        }
    );

    assert_constant_initializer_cycle_diagnostic(path, 3, "STATUS_MASK");
}

void test_constant_initializer_await_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_constant_await_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const MASKED: UInt32 = await STATUS_MASK",
            "const STATUS_MASK: UInt32 = 0xFF",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 2, "await expression");
}

void test_constant_initializer_task_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_constant_task_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const MASKED: UInt32 = task",
            "    1",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 2, "task expression");
}

void test_constant_initializer_thread_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_constant_thread_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const MASKED: UInt32 = thread",
            "    1",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 2, "thread expression");
}

void test_constant_initializer_unsafe_intrinsic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_constant_unsafe_intrinsic_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const UART_STATUS: Address = 0x4000_1000",
            "const STATUS: UInt32 = raw_read(UART_STATUS)",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 3, "unsafe intrinsic 'raw_read'");
}

void test_constant_initializer_unsafe_function_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_constant_unsafe_function_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "unsafe function read_word(p: Address) -> UInt32",
            "    return raw_read(p)",
            "const UART_STATUS: Address = 0x4000_1000",
            "const STATUS: UInt32 = read_word(UART_STATUS)",
        }
    );

    assert_constant_initializer_unsafe_call_diagnostic(path, 5, "read_word");
}

void test_constant_initializer_function_call_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_constant_function_call_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "function mask(value: UInt32) -> UInt32",
            "    return value bit_and 0xFF",
            "const STATUS: UInt32 = mask(0xFFFF)",
        }
    );

    assert_constant_initializer_function_call_diagnostic(path, 4, "mask");
}

void test_constant_initializer_method_call_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_constant_method_call_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "extend Int64",
            "    function low_byte(this: shared This) -> UInt32",
            "        return 0xFF as UInt32",
            "const STATUS_WORD: Int64 = 0xFFFF",
            "const STATUS: UInt32 = STATUS_WORD.low_byte()",
        }
    );

    assert_constant_initializer_method_call_diagnostic(path, 6, "low_byte");
}

void test_zero_payload_choice_constant_initializer_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_constant_zero_payload_success.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "choice Mode",
            "    Off",
            "    On",
            "const DEFAULT_MODE: Mode = Off",
        }
    );

    assert_fixture_success(path);
}

void test_payload_choice_constant_initializer_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_constant_payload_success.or";
    write_status_choice_constant_fixture(path, "const DEFAULT_STATUS: Status = Ready(0xFF)");

    assert_fixture_success(path);
}

void test_generic_choice_constant_initializer_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_generic_choice_constant_success.or";
    write_maybe_choice_constant_fixture(path, "const DEFAULT_VALUE: Maybe<UInt32> = Some(0xFF)");

    assert_fixture_success(path);
}

void test_generic_choice_constant_initializer_payload_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_choice_constant_payload_failure.or";
    write_maybe_choice_constant_fixture(path, "const DEFAULT_VALUE: Maybe<UInt32> = Some(true)");

    assert_choice_constructor_payload_mismatch_diagnostic(path, 5, "Bool", "UInt32");
}

void test_generic_choice_constant_repeated_payload_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_choice_repeated_payload_success.or";
    write_maybe_choice_constant_fixture_with_declarations(
        path,
        "const DEFAULT_VALUE: Both<UInt16> = Pair(1 as UInt16, 2 as Int16)",
        {
            "choice Both<T>",
            "    Pair(left: T, right: T)",
        }
    );

    assert_fixture_success(path);
}

void test_generic_choice_constant_repeated_payload_conflict_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_generic_choice_repeated_payload_failure.or";
    write_maybe_choice_constant_fixture_with_declarations(
        path,
        "const DEFAULT_VALUE: Both<Header> = Pair(Header([1, 2], 1), OtherHeader([1, 2], 1))",
        {
            "record Header",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "record OtherHeader",
            "    magic: Array<UInt32, 2>",
            "    version: UInt16",
            "choice Both<T>",
            "    Pair(left: T, right: T)",
        }
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 13, "OtherHeader", "Header");
}

void test_ordinary_choice_constructor_annotated_binding_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_binding_constructor_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> UInt32",
            "    let value: Maybe<UInt32> = Some(0xFF)",
            "    return 1",
        }
    );

    assert_fixture_success(path);
}

void test_choice_constructor_unannotated_binding_infers_type_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_unannotated_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo() -> UInt32",
            "    let value = Some(1 as UInt32)",
            "    return consume(value)",
        }
    );

    assert_fixture_success(path);
}

void test_choice_constructor_unannotated_binding_infers_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_unannotated_binding_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo() -> UInt32",
            "    let value = Some(true)",
            "    return consume(value)",
        }
    );

    assert_function_argument_type_mismatch_diagnostic(path, 9, "value", "Maybe<Bool>", "Maybe<UInt32>");
}

void test_choice_constructor_unannotated_zero_payload_requires_expected_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_unannotated_zero_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> UInt32",
            "    let value = Empty",
            "    return 1",
        }
    );

    assert_choice_constructor_expected_type_required_diagnostic(path, 6, "Empty");
}

void test_choice_constructor_unannotated_ambiguous_name_requires_expected_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_unannotated_ambiguous_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice LocalStatus",
            "    Ready(value: UInt32)",
            "choice RemoteStatus",
            "    Ready(value: UInt32)",
            "function demo() -> UInt32",
            "    let value = Ready(1 as UInt32)",
            "    return 1",
        }
    );

    assert_choice_constructor_expected_type_required_diagnostic(path, 7, "Ready");
}

void test_ordinary_choice_constructor_return_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_return_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    return Some(true)",
        }
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 6, "Bool", "UInt32");
}

void test_ordinary_choice_constructor_final_expression_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_expression_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    Some(true)",
        }
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 6, "Bool", "UInt32");
}

void test_final_if_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_if_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        ordinary_final_if_lines("true", "1 as UInt32")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 4, "Bool", "UInt32");
}

void test_final_switch_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_switch_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        ordinary_final_switch_lines("true", "1 as UInt32")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 4, "Bool", "UInt32");
}

void test_final_if_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_if_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        ordinary_final_if_lines("1 as UInt32", "2 as UInt32")
    );

    assert_fixture_success(path);
}

void test_final_switch_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_switch_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        ordinary_final_switch_lines("1 as UInt32", "2 as UInt32")
    );

    assert_fixture_success(path);
}

void test_final_if_return_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_if_return_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        ordinary_final_if_lines("return true", "return 1 as UInt32")
    );

    assert_return_expression_type_mismatch_diagnostic(path, 4, "Bool", "UInt32");
}

void test_final_switch_return_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_switch_return_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        ordinary_final_switch_lines("return true", "return 1 as UInt32")
    );

    assert_return_expression_type_mismatch_diagnostic(path, 4, "Bool", "UInt32");
}

void test_final_if_return_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_if_return_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        ordinary_final_if_lines("return 1 as UInt32", "return 2 as UInt32")
    );

    assert_fixture_success(path);
}

void test_final_switch_return_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_switch_return_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        ordinary_final_switch_lines("return 1 as UInt32", "return 2 as UInt32")
    );

    assert_fixture_success(path);
}

void test_final_ternary_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_ternary_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo(flag: Bool) -> UInt32",
            "    flag ? true : 1 as UInt32",
        }
    );

    assert_final_expression_type_mismatch_diagnostic(path, 3, "Bool", "UInt32");
}

void test_final_ternary_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_ternary_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo(flag: Bool) -> UInt32",
            "    flag ? 1 as UInt32 : 2 as UInt32",
        }
    );

    assert_fixture_success(path);
}

void test_final_ternary_return_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_ternary_return_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo(flag: Bool) -> UInt32",
            "    return flag ? true : 1 as UInt32",
        }
    );

    assert_return_expression_type_mismatch_diagnostic(path, 3, "Bool", "UInt32");
}

void test_final_ternary_return_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_ternary_return_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo(flag: Bool) -> UInt32",
            "    return flag ? 1 as UInt32 : 2 as UInt32",
        }
    );

    assert_fixture_success(path);
}

void test_final_if_without_else_requires_function_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_if_without_else_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo(flag: Bool) -> UInt32",
            "    if flag",
            "        1 as UInt32",
        }
    );

    assert_function_final_value_required_diagnostic(path, 3);
}

void test_final_unsafe_block_without_value_requires_function_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_unsafe_without_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo() -> UInt32",
            "    unsafe",
            "        let value = 1 as UInt32",
        }
    );

    assert_function_final_value_required_diagnostic(path, 3);
}

void test_final_switch_non_value_case_requires_function_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_final_switch_non_value_case_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo(flag: Bool) -> UInt32",
            "    switch flag",
            "        true =>",
            "            let value = 1 as UInt32",
            "        false => 2 as UInt32",
        }
    );

    assert_function_final_value_required_diagnostic(path, 3);
}

void test_non_unit_empty_return_requires_value_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_non_unit_empty_return_failure.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo() -> UInt32",
            "    return",
        }
    );

    assert_empty_return_value_required_diagnostic(path, 3, "UInt32");
}

void test_unit_empty_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unit_empty_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo() -> Unit",
            "    return",
        }
    );

    assert_fixture_success(path);
}

void test_unit_final_if_without_else_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unit_final_if_without_else_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo(flag: Bool) -> Unit",
            "    if flag",
            "        let value = 1 as UInt32",
        }
    );

    assert_fixture_success(path);
}

void test_unit_final_unsafe_without_value_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unit_final_unsafe_without_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo() -> Unit",
            "    unsafe",
            "        let value = 1 as UInt32",
        }
    );

    assert_fixture_success(path);
}

void test_unit_final_switch_non_value_case_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_unit_final_switch_non_value_case_success.or";
    write_concurrency_fixture(
        path,
        "demo.branch",
        {
            "function demo(flag: Bool) -> Unit",
            "    switch flag",
            "        true =>",
            "            let value = 1 as UInt32",
            "        false =>",
            "            let value = 2 as UInt32",
        }
    );

    assert_fixture_success(path);
}

void test_choice_constructor_final_if_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_if_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        choice_final_if_lines("Some(true)", "Empty")
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 7, "Bool", "UInt32");
}

void test_choice_constructor_final_switch_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_switch_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        choice_final_switch_lines("Some(true)", "Empty")
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 7, "Bool", "UInt32");
}

void test_choice_constructor_final_if_return_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_if_return_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        choice_final_if_lines("return Some(true)", "return Empty")
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 7, "Bool", "UInt32");
}

void test_choice_constructor_final_switch_return_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_switch_return_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        choice_final_switch_lines("return Some(true)", "return Empty")
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 7, "Bool", "UInt32");
}

void test_choice_constructor_final_if_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_final_if_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        choice_final_if_lines("Some(1 as UInt32)", "Empty")
    );

    assert_fixture_success(path);
}

void test_choice_constructor_final_switch_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_final_switch_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        choice_final_switch_lines("Some(1 as UInt32)", "Empty")
    );

    assert_fixture_success(path);
}

void test_choice_constructor_final_if_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_final_if_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        choice_final_if_lines("return Some(1 as UInt32)", "return Empty")
    );

    assert_fixture_success(path);
}

void test_choice_constructor_final_switch_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_final_switch_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        choice_final_switch_lines("return Some(1 as UInt32)", "return Empty")
    );

    assert_fixture_success(path);
}

void test_choice_constructor_final_ternary_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_ternary_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    flag ? Some(true) : Empty",
        }
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 6, "Bool", "UInt32");
}

void test_choice_constructor_final_ternary_return_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_ternary_return_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    return flag ? Some(true) : Empty",
        }
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 6, "Bool", "UInt32");
}

void test_choice_constructor_final_ternary_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_final_ternary_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    flag ? Some(1 as UInt32) : Empty",
        }
    );

    assert_fixture_success(path);
}

void test_choice_constructor_final_ternary_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_final_ternary_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    return flag ? Some(1 as UInt32) : Empty",
        }
    );

    assert_fixture_success(path);
}

void test_choice_constructor_nested_final_ternary_payload_types() {
    auto unsafe_failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_unsafe_final_ternary_payload_failure.or";
    write_concurrency_fixture(
        unsafe_failure_path,
        "demo.choices",
        choice_final_unsafe_block_ternary_lines("flag ? Some(true) : Empty")
    );
    assert_choice_constructor_payload_mismatch_diagnostic(unsafe_failure_path, 7, "Bool", "UInt32");

    auto unsafe_success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_unsafe_final_ternary_success.or";
    write_concurrency_fixture(
        unsafe_success_path,
        "demo.choices",
        choice_final_unsafe_block_ternary_lines("flag ? Some(1 as UInt32) : Empty")
    );
    assert_fixture_success(unsafe_success_path);

    auto if_failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_if_ternary_payload_failure.or";
    write_concurrency_fixture(
        if_failure_path,
        "demo.choices",
        choice_final_if_ternary_lines("Some(true)", "Empty")
    );
    assert_choice_constructor_payload_mismatch_diagnostic(if_failure_path, 7, "Bool", "UInt32");

    auto if_success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_if_ternary_success.or";
    write_concurrency_fixture(
        if_success_path,
        "demo.choices",
        choice_final_if_ternary_lines("Some(1 as UInt32)", "Empty")
    );
    assert_fixture_success(if_success_path);

    auto switch_failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_switch_ternary_payload_failure.or";
    write_concurrency_fixture(
        switch_failure_path,
        "demo.choices",
        choice_final_switch_ternary_lines("Some(true)", "Empty")
    );
    assert_choice_constructor_payload_mismatch_diagnostic(switch_failure_path, 7, "Bool", "UInt32");

    auto switch_success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_final_switch_ternary_success.or";
    write_concurrency_fixture(
        switch_success_path,
        "demo.choices",
        choice_final_switch_ternary_lines("Some(1 as UInt32)", "Empty")
    );
    assert_fixture_success(switch_success_path);
}

void test_choice_constructor_ternary_annotated_binding_payload_types() {
    auto failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_ternary_binding_payload_failure.or";
    write_concurrency_fixture(
        failure_path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    let value: Maybe<UInt32> = flag ? Some(true) : Empty",
            "    return 1",
        }
    );
    assert_choice_constructor_payload_mismatch_diagnostic(failure_path, 6, "Bool", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_ternary_binding_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    let value: Maybe<UInt32> = flag ? Some(1 as UInt32) : Empty",
            "    return 1",
        }
    );
    assert_fixture_success(success_path);
}

void test_choice_constructor_ternary_assignment_payload_types() {
    auto failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_ternary_assignment_payload_failure.or";
    write_concurrency_fixture(
        failure_path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    var value: Maybe<UInt32> = Empty",
            "    value = flag ? Some(true) : Empty",
            "    return 1",
        }
    );
    assert_choice_constructor_payload_mismatch_diagnostic(failure_path, 7, "Bool", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_ternary_assignment_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    var value: Maybe<UInt32> = Empty",
            "    value = flag ? Some(1 as UInt32) : Empty",
            "    return 1",
        }
    );
    assert_fixture_success(success_path);
}

void test_choice_constructor_ternary_call_argument_payload_types() {
    auto failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_ternary_call_argument_failure.or";
    write_concurrency_fixture(
        failure_path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo(flag: Bool) -> UInt32",
            "    return consume(flag ? Some(true) : Empty)",
        }
    );
    assert_choice_constructor_payload_mismatch_diagnostic(failure_path, 8, "Bool", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_ternary_call_argument_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo(flag: Bool) -> UInt32",
            "    return consume(flag ? Some(1 as UInt32) : Empty)",
        }
    );
    assert_fixture_success(success_path);
}

void test_ordinary_choice_constructor_assignment_payload_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_assignment_payload_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> UInt32",
            "    var value: Maybe<UInt32> = Empty",
            "    value = Some(true)",
            "    return 1",
        }
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 7, "Bool", "UInt32");
}

void test_ordinary_choice_constructor_call_argument_arity_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_call_argument_arity_failure.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo() -> UInt32",
            "    return consume(Some())",
        }
    );

    assert_choice_constructor_arity_diagnostic(path, 8, "Some", 1, 0);
}

void test_zero_payload_choice_constructor_annotated_binding_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_zero_payload_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> UInt32",
            "    let value: Maybe<UInt32> = Empty",
            "    return 1",
        }
    );

    assert_fixture_success(path);
}

void test_zero_payload_choice_constructor_return_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_zero_payload_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    return Empty",
        }
    );

    assert_fixture_success(path);
}

void test_zero_payload_choice_constructor_final_expression_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_zero_payload_final_expression_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    Empty",
        }
    );

    assert_fixture_success(path);
}

void test_zero_payload_choice_constructor_assignment_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_zero_payload_assignment_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> UInt32",
            "    var value: Maybe<UInt32> = Some(1 as UInt32)",
            "    value = Empty",
            "    return 1",
        }
    );

    assert_fixture_success(path);
}

void test_zero_payload_choice_constructor_call_argument_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_choice_zero_payload_call_argument_success.or";
    write_concurrency_fixture(
        path,
        "demo.choices",
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo() -> UInt32",
            "    return consume(Empty)",
        }
    );

    assert_fixture_success(path);
}

void test_nested_choice_constant_initializer_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_nested_choice_constant_success.or";
    write_boxed_maybe_choice_constant_fixture(
        path,
        "const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Some(0xFF))"
    );

    assert_fixture_success(path);
}

void test_nested_choice_constant_initializer_payload_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_nested_choice_constant_payload_failure.or";
    write_boxed_maybe_choice_constant_fixture(
        path,
        "const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Some(true))"
    );

    assert_choice_constructor_payload_mismatch_diagnostic(path, 7, "Bool", "UInt32");
}

void test_nested_zero_payload_choice_constant_initializer_arity_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_nested_zero_payload_choice_constant_arity_failure.or";
    write_boxed_maybe_choice_constant_fixture(
        path,
        "const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Empty(1))"
    );

    assert_choice_constructor_arity_diagnostic(path, 7, "Empty", 0, 1);
}

void test_nested_choice_constant_initializer_wrong_choice_type_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_choice_constant_wrong_choice_failure.or";
    write_boxed_maybe_result_choice_constant_fixture(
        path,
        "const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Error(\"missing\"))"
    );

    assert_choice_constructor_declared_type_mismatch_diagnostic(path, 10, "Error", "Maybe<UInt32>");
}

void test_nested_choice_constant_initializer_unknown_constructor_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_nested_choice_constant_unknown_failure.or";
    write_boxed_maybe_choice_constant_fixture(
        path,
        "const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Missing(1))"
    );

    assert_choice_constructor_unknown_diagnostic(path, 7, "Missing", "Maybe<UInt32>");
}

void test_choice_constant_initializer_wrong_choice_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_constant_wrong_choice_failure.or";
    write_maybe_result_choice_constant_fixture(path, "const DEFAULT_VALUE: Maybe<UInt32> = Error(\"missing\")");

    assert_choice_constructor_declared_type_mismatch_diagnostic(path, 8, "Error", "Maybe<UInt32>");
}

void test_choice_constant_initializer_unknown_constructor_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_constant_unknown_failure.or";
    write_maybe_choice_constant_fixture(path, "const DEFAULT_VALUE: Maybe<UInt32> = Missing(1)");

    assert_choice_constructor_unknown_diagnostic(path, 5, "Missing", "Maybe<UInt32>");
}

void test_choice_constant_initializer_arity_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_constant_arity_failure.or";
    write_status_choice_constant_fixture(path, "const DEFAULT_STATUS: Status = Ready()");

    assert_choice_constructor_arity_diagnostic(path, 5, "Ready", 1, 0);
}

void test_zero_payload_choice_constant_initializer_arity_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_zero_payload_choice_constant_arity_failure.or";
    write_status_choice_constant_fixture(path, "const DEFAULT_STATUS: Status = Empty(1)");

    assert_choice_constructor_arity_diagnostic(path, 5, "Empty", 0, 1);
}

void test_choice_constant_initializer_payload_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_choice_constant_payload_failure.or";
    write_status_choice_constant_fixture(path, "const DEFAULT_STATUS: Status = Ready(true)");

    assert_choice_constructor_payload_mismatch_diagnostic(path, 5, "Bool", "UInt32");
}

void test_constant_initializer_pointer_construction_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_constant_pointer_construction_failure.or";
    write_concurrency_fixture(
        path,
        "demo.consts",
        {
            "const UART_STATUS: Address = 0x4000_1000",
            "const STATUS: Pointer<UInt32> = Pointer(UART_STATUS)",
        }
    );

    assert_constant_initializer_runtime_construct_diagnostic(path, 3, "Pointer construction");
}

void test_nested_address_of_and_raw_offset_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_nested_address_of_raw_offset_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function poke(buf: exclusive Buffer, value: Byte) -> Unit",
            "    let p = address_of(buf.data[0])",
            "    raw_write(raw_offset(p, 1), value)",
        }
    );

    assert_fixture_success(path);
}

void test_index_access_noninteger_index_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_index_access_noninteger_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_byte(device: Device, value: Byte) -> Unit",
            "    raw_write(device.ptrs[\"one\"], value)",
        }
    );

    assert_index_access_integer_index_diagnostic(path, 5);
}

void test_index_access_integer_index_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_index_access_integer_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_byte(device: Device, index: UInt32, value: Byte) -> Unit",
            "    raw_write(device.ptrs[index], value)",
        }
    );

    assert_fixture_success(path);
}

void test_call_unsafe_function_outside_unsafe_context_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_call_unsafe_function_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Address) -> UInt32",
            "    return raw_read(p)",
            "function read_twice(p: Address) -> UInt32",
            "    return read_word(p)",
        }
    );

    assert_unsafe_function_call_context_diagnostic(path, 5, "read_word");
}

void test_call_unsafe_function_inside_unsafe_block_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_call_unsafe_function_block_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Address) -> UInt32",
            "    return raw_read(p)",
            "function copy_word(p: Address) -> UInt32",
            "    unsafe",
            "        return read_word(p)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_construction_outside_unsafe_context_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function read_byte(addr: Address) -> Byte",
            "    let p = Pointer(addr)",
            "    return raw_read(p)",
        }
    );

    auto diagnostics = analyze_orison_fixture(path);
    assert_pointer_construction_unsafe_boundary_diagnostics(diagnostics);
}

void test_pointer_construction_inside_unsafe_block_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_block_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function scribble(addr: Address) -> Unit",
            "    unsafe",
            "        let p = Pointer(addr)",
            "        raw_write(p, 0)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_construction_without_argument_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_noarg_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let p = Pointer()",
            "    return raw_read(p)",
        }
    );

    assert_pointer_construction_single_source_diagnostic(path, 3);
}

void test_pointer_construction_with_multiple_arguments_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_multiarg_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(addr: Address) -> Byte",
            "    let p = Pointer(addr, addr)",
            "    return raw_read(p)",
        }
    );

    assert_pointer_construction_single_source_diagnostic(path, 3);
}

void test_pointer_construction_with_nonaddress_argument_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_nonaddress_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let p = Pointer(\"text\")",
            "    return raw_read(p)",
        }
    );

    assert_pointer_construction_address_source_diagnostic(path, 3);
}

void test_pointer_construction_with_address_of_argument_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_construction_addressof_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function first_ptr(buf: exclusive Buffer) -> Pointer<Byte>",
            "    let p = Pointer(address_of(buf.data[0]))",
            "    return p",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_typed_binding_with_mismatched_address_of_source_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_addressof_source_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_word_ptr(buf: Buffer) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = Pointer(address_of(buf.data[0]))",
            "    return p",
        }
    );

    assert_pointer_construction_source_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_pointer_typed_binding_with_matching_address_of_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_addressof_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_byte_ptr(buf: Buffer) -> Pointer<Byte>",
            "    let p: Pointer<Byte> = Pointer(address_of(buf.data[0]))",
            "    return p",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_with_same_width_address_of_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_same_width_addressof_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function status_ptr(device: Device) -> Pointer<UInt32>",
            "    return Pointer(address_of(device.registers.status))",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_typed_binding_with_nonpointer_initializer_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_typed_binding_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let p: Pointer<Byte> = \"text\"",
            "    return 0",
        }
    );

    assert_pointer_typed_binding_initializer_diagnostic(path, 3);
}

void test_pointer_typed_binding_with_pointer_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_typed_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte(base: Pointer<Byte>) -> Byte",
            "    let p: Pointer<Byte> = raw_offset(base, 1)",
            "    return raw_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_typed_binding_with_mismatched_raw_offset_source_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_rawoffset_source_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = raw_offset(base, 1)",
            "    return p",
        }
    );

    assert_raw_offset_source_pointee_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_pointer_typed_binding_with_matching_raw_offset_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_rawoffset_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    let p: Pointer<Byte> = raw_offset(base, 1)",
            "    return p",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_typed_binding_ternary_raw_offset_source_types() {
    auto failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_binding_ternary_rawoffset_failure.or";
    write_concurrency_fixture(
        failure_path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        }
    );
    assert_raw_offset_source_pointee_mismatch_diagnostic(failure_path, 3, "Byte", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_binding_ternary_rawoffset_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.unsafe",
        {
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    let p: Pointer<Byte> = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        }
    );
    assert_fixture_success(success_path);
}

void test_pointer_assignment_ternary_raw_offset_source_types() {
    auto failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_assignment_ternary_rawoffset_failure.or";
    write_concurrency_fixture(
        failure_path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    var p: Pointer<UInt32> = raw_offset(other, 1)",
            "    p = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        }
    );
    assert_raw_offset_source_pointee_mismatch_diagnostic(failure_path, 4, "Byte", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_assignment_ternary_rawoffset_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.unsafe",
        {
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    var p: Pointer<Byte> = raw_offset(other, 1)",
            "    p = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        }
    );
    assert_fixture_success(success_path);
}

void test_pointer_call_argument_ternary_raw_offset_source_types() {
    auto failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_call_ternary_rawoffset_failure.or";
    write_concurrency_fixture(
        failure_path,
        "demo.unsafe",
        {
            "unsafe function consume(ptr: Pointer<UInt32>) -> UInt32",
            "    return 1",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    return consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        }
    );
    assert_raw_offset_source_pointee_mismatch_diagnostic(failure_path, 5, "Byte", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_call_ternary_rawoffset_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.unsafe",
        {
            "unsafe function consume(ptr: Pointer<Byte>) -> UInt32",
            "    return 1",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> UInt32",
            "    return consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        }
    );
    assert_fixture_success(success_path);
}

void test_pointer_method_argument_ternary_raw_offset_source_types() {
    auto failure_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_method_ternary_rawoffset_failure.or";
    write_concurrency_fixture(
        failure_path,
        "demo.unsafe",
        {
            "record Device",
            "    status: UInt32",
            "extend Device",
            "    function consume(this: shared This, ptr: Pointer<UInt32>) -> UInt32",
            "        return 1",
            "unsafe function demo(device: Device, flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    return device.consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        }
    );
    assert_raw_offset_source_pointee_mismatch_diagnostic(failure_path, 8, "Byte", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_method_ternary_rawoffset_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.unsafe",
        {
            "record Device",
            "    status: UInt32",
            "extend Device",
            "    function consume(this: shared This, ptr: Pointer<Byte>) -> UInt32",
            "        return 1",
            "unsafe function demo(device: Device, flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> UInt32",
            "    return device.consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        }
    );
    assert_fixture_success(success_path);
}

void test_pointer_return_with_same_width_raw_offset_source_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_same_width_rawoffset_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_typed_binding_result_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_typed_binding_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> UInt32",
            "    let value: UInt32 = raw_read(p)",
            "    return value",
        }
    );

    assert_raw_read_binding_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_raw_read_typed_binding_result_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_typed_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    let value: Byte = raw_read(p)",
            "    return value",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_typed_binding_same_width_integer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_typed_binding_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> Int32",
            "    let value: UInt32 = raw_read(p)",
            "    return value as Int32",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_typed_binding_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_typed_binding_pointer_sized_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> IntSize",
            "    let value: IntSize = raw_read(p)",
            "    return value",
        }
    );

    assert_raw_read_binding_mismatch_diagnostic(path, 3, "Byte", "IntSize");
}

void test_pointer_typed_binding_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_typed_binding_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte() -> Byte",
            "    let source = \"text\"",
            "    let p: Pointer<Byte> = source",
            "    return 0",
        }
    );

    assert_pointer_typed_binding_initializer_diagnostic(path, 4);
}

void test_pointer_typed_binding_with_mismatched_field_pointer_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_field_pointer_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptr: Pointer<Byte>",
            "unsafe function next_ptr(device: Device) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = device.ptr",
            "    return p",
        }
    );

    assert_pointer_typed_binding_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_pointer_typed_binding_with_same_width_field_pointer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_typed_binding_same_width_field_pointer_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptr: Pointer<Int32>",
            "unsafe function next_ptr(device: Device) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = device.ptr",
            "    return p",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_with_nonpointer_expression_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    return \"text\"",
        }
    );

    assert_pointer_return_structural_diagnostic(path, 3);
}

void test_pointer_final_expression_with_nonpointer_expression_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_final_expression_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    \"text\"",
        }
    );

    assert_pointer_return_structural_diagnostic(path, 3);
}

void test_pointer_return_with_pointer_expression_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_final_expression_with_pointer_expression_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_final_expression_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    raw_offset(base, 1)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_with_mismatched_raw_offset_source_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_rawoffset_source_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
        }
    );

    assert_raw_offset_source_pointee_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_pointer_return_with_matching_raw_offset_source_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_rawoffset_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_ternary_raw_offset_source_types() {
    auto mismatch_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_ternary_rawoffset_failure.or";
    write_concurrency_fixture(
        mismatch_path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        }
    );
    assert_raw_offset_source_pointee_mismatch_diagnostic(mismatch_path, 3, "Byte", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_ternary_rawoffset_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.unsafe",
        {
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    return flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        }
    );
    assert_fixture_success(success_path);
}

void test_pointer_final_ternary_raw_offset_source_types() {
    auto mismatch_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_final_ternary_rawoffset_failure.or";
    write_concurrency_fixture(
        mismatch_path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        }
    );
    assert_raw_offset_source_pointee_mismatch_diagnostic(mismatch_path, 3, "Byte", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_final_ternary_rawoffset_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.unsafe",
        {
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        }
    );
    assert_fixture_success(success_path);
}

void test_pointer_nested_final_container_ternary_raw_offset_source_types() {
    auto unsafe_mismatch_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_unsafe_final_ternary_rawoffset_failure.or";
    write_concurrency_fixture(
        unsafe_mismatch_path,
        "demo.unsafe",
        {
            "function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    unsafe",
            "        flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        }
    );
    assert_raw_offset_source_pointee_mismatch_diagnostic(unsafe_mismatch_path, 4, "Byte", "UInt32");

    auto if_mismatch_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_final_if_ternary_rawoffset_failure.or";
    write_concurrency_fixture(
        if_mismatch_path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    if flag",
            "        flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    else",
            "        raw_offset(other, 1)",
        }
    );
    assert_raw_offset_source_pointee_mismatch_diagnostic(if_mismatch_path, 4, "Byte", "UInt32");

    auto switch_success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_final_switch_ternary_rawoffset_success.or";
    write_concurrency_fixture(
        switch_success_path,
        "demo.unsafe",
        {
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    switch flag",
            "        true => flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "        false => raw_offset(other, 1)",
        }
    );
    assert_fixture_success(switch_success_path);
}

void test_pointer_return_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    let source = \"text\"",
            "    return source",
        }
    );

    assert_pointer_return_structural_diagnostic(path, 4);
}

void test_pointer_return_with_mismatched_helper_pointer_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_helper_pointer_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return base",
            "unsafe function word_ptr(base: Pointer<Byte>) -> Pointer<UInt32>",
            "    return byte_ptr(base)",
        }
    );

    assert_pointer_return_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_pointer_return_with_same_width_helper_pointer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_pointer_return_same_width_helper_pointer_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function wordish_ptr(base: Pointer<Int32>) -> Pointer<Int32>",
            "    return base",
            "unsafe function word_ptr(base: Pointer<Int32>) -> Pointer<UInt32>",
            "    return wordish_ptr(base)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_generic_helper_returned_pointer_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_helper_returned_pointer_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>",
            "    return base",
            "unsafe function write_word(base: Pointer<Int32>, value: UInt32) -> Unit",
            "    raw_write(id_ptr(base), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_generic_helper_returned_pointer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_helper_returned_pointer_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function id_ptr<T>(base: Pointer<T>) -> Pointer<T>",
            "    return base",
            "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(id_ptr(base), value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_address_return_with_generic_helper_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_return_with_generic_helper_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function id<T>(value: T) -> T",
            "    return value",
            "record Device",
            "    base: Address",
            "function read_base(device: Device) -> Address",
            "    return id(device.base)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_generic_receiver_method_pointer_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_receiver_method_pointer_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    id: Int64",
            "extend Device<T>",
            "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>",
            "        return base",
            "unsafe function write_word(device: Device<Int32>, base: Pointer<Int32>, value: UInt32) -> Unit",
            "    raw_write(device.ptr(base), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_generic_receiver_method_pointer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_receiver_method_pointer_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    id: Int64",
            "extend Device<T>",
            "    function ptr(this: shared This, base: Pointer<T>) -> Pointer<T>",
            "        return base",
            "unsafe function write_word(device: Device<Byte>, base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(device.ptr(base), value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 8, "UInt32", "Byte");
}

void test_raw_write_generic_record_pointer_field_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_record_pointer_field_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    ptr: Pointer<T>",
            "unsafe function write_word(device: Device<Int32>, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_generic_record_pointer_field_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_record_pointer_field_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device<T>",
            "    ptr: Pointer<T>",
            "unsafe function write_word(device: Device<Byte>, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_raw_write_generic_record_scalar_field_same_width_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_generic_record_scalar_field_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Box<T>",
            "    value: T",
            "unsafe function write_word(box: Box<Int32>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, box.value)",
        }
    );

    assert_fixture_success(path);
}

void test_address_return_with_generic_record_field_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_return_with_generic_record_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Box<T>",
            "    value: T",
            "function read_base(box: Box<Address>) -> Address",
            "    return box.value",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_with_mismatched_address_of_source_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_addressof_source_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_word_ptr(buf: Buffer) -> Pointer<UInt32>",
            "    return Pointer(address_of(buf.data[0]))",
        }
    );

    assert_pointer_construction_source_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_pointer_return_with_matching_address_of_source_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_addressof_source_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_byte_ptr(buf: Buffer) -> Pointer<Byte>",
            "    return Pointer(address_of(buf.data[0]))",
        }
    );

    assert_fixture_success(path);
}

void test_pointer_return_ternary_pointer_construction_source_types() {
    auto mismatch_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_ternary_pointer_source_failure.or";
    write_concurrency_fixture(
        mismatch_path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_word_ptr(flag: Bool, buf: Buffer, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return flag ? Pointer(address_of(buf.data[0])) : raw_offset(other, 1)",
        }
    );
    assert_pointer_construction_source_mismatch_diagnostic(mismatch_path, 5, "Byte", "UInt32");

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_pointer_return_ternary_pointer_source_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_byte_ptr(flag: Bool, buf: Buffer) -> Pointer<Byte>",
            "    return flag ? Pointer(address_of(buf.data[0])) : Pointer(address_of(buf.data[1]))",
        }
    );
    assert_fixture_success(success_path);
}

void test_raw_read_return_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_return_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_read(p)",
        }
    );

    assert_raw_read_result_mismatch_diagnostic(path, 3, "Byte", "Pointer<Byte>");
}

void test_raw_read_final_expression_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_direct_mismatch_lines("raw_read")
    );

    assert_raw_read_result_mismatch_diagnostic(path, 3, "Byte", "Pointer<Byte>");
}

void test_raw_read_rebound_final_expression_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_rebound_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_rebound_mismatch_lines("raw_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 4, "Byte", "UInt32");
}

void test_raw_read_unsafe_block_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_unsafe_block_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_unsafe_block_direct_mismatch_lines("raw_read")
    );

    assert_raw_read_result_mismatch_diagnostic(path, 4, "Byte", "Pointer<Byte>");
}

void test_raw_read_unsafe_block_rebound_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_unsafe_block_rebound_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_unsafe_block_rebound_mismatch_lines("raw_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_raw_read_branch_merged_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_branch_merged_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_branch_mismatch_lines("raw_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 8, "Byte", "UInt32");
}

void test_raw_read_switch_merged_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_switch_merged_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_switch_mismatch_lines("raw_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 7, "Byte", "UInt32");
}

void test_raw_read_final_if_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_final_if_expression_type_failure.or";
    write_concurrency_fixture(path, "demo.unsafe", low_level_final_if_read_mismatch_lines("raw_read"));

    assert_raw_read_result_mismatch_diagnostic(path, 4, "Byte", "UInt32");
}

void test_raw_read_final_switch_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_final_switch_expression_type_failure.or";
    write_concurrency_fixture(path, "demo.unsafe", low_level_final_switch_read_mismatch_lines("raw_read"));

    assert_raw_read_result_mismatch_diagnostic(path, 4, "Byte", "UInt32");
}

void test_low_level_read_nested_final_container_expression_types(std::string_view intrinsic) {
    auto unsafe_if_failure_path = std::filesystem::temp_directory_path() /
                                  ("orison_semantics_" + std::string(intrinsic) +
                                   "_unsafe_block_final_if_expression_type_failure.or");
    write_concurrency_fixture(
        unsafe_if_failure_path,
        "demo.unsafe",
        low_level_final_unsafe_block_if_read_mismatch_lines(intrinsic)
    );
    assert_low_level_read_result_mismatch_diagnostic(unsafe_if_failure_path, 5, intrinsic, "Byte", "UInt32");

    auto unsafe_if_success_path = std::filesystem::temp_directory_path() /
                                  ("orison_semantics_" + std::string(intrinsic) +
                                   "_unsafe_block_final_if_expression_type_success.or");
    write_concurrency_fixture(
        unsafe_if_success_path,
        "demo.unsafe",
        low_level_final_unsafe_block_if_read_success_lines(intrinsic)
    );
    assert_fixture_success(unsafe_if_success_path);

    auto unsafe_switch_failure_path = std::filesystem::temp_directory_path() /
                                      ("orison_semantics_" + std::string(intrinsic) +
                                       "_unsafe_block_final_switch_expression_type_failure.or");
    write_concurrency_fixture(
        unsafe_switch_failure_path,
        "demo.unsafe",
        low_level_final_unsafe_block_switch_read_mismatch_lines(intrinsic)
    );
    assert_low_level_read_result_mismatch_diagnostic(unsafe_switch_failure_path, 5, intrinsic, "Byte", "UInt32");

    auto unsafe_switch_success_path = std::filesystem::temp_directory_path() /
                                      ("orison_semantics_" + std::string(intrinsic) +
                                       "_unsafe_block_final_switch_expression_type_success.or");
    write_concurrency_fixture(
        unsafe_switch_success_path,
        "demo.unsafe",
        low_level_final_unsafe_block_switch_read_success_lines(intrinsic)
    );
    assert_fixture_success(unsafe_switch_success_path);

    auto if_unsafe_failure_path = std::filesystem::temp_directory_path() /
                                  ("orison_semantics_" + std::string(intrinsic) +
                                   "_final_if_unsafe_block_expression_type_failure.or");
    write_concurrency_fixture(
        if_unsafe_failure_path,
        "demo.unsafe",
        low_level_final_if_unsafe_block_read_mismatch_lines(intrinsic)
    );
    assert_low_level_read_result_mismatch_diagnostic(if_unsafe_failure_path, 5, intrinsic, "Byte", "UInt32");

    auto if_unsafe_success_path = std::filesystem::temp_directory_path() /
                                  ("orison_semantics_" + std::string(intrinsic) +
                                   "_final_if_unsafe_block_expression_type_success.or");
    write_concurrency_fixture(
        if_unsafe_success_path,
        "demo.unsafe",
        low_level_final_if_unsafe_block_read_success_lines(intrinsic)
    );
    assert_fixture_success(if_unsafe_success_path);

    auto switch_unsafe_failure_path = std::filesystem::temp_directory_path() /
                                      ("orison_semantics_" + std::string(intrinsic) +
                                       "_final_switch_unsafe_block_expression_type_failure.or");
    write_concurrency_fixture(
        switch_unsafe_failure_path,
        "demo.unsafe",
        low_level_final_switch_unsafe_block_read_mismatch_lines(intrinsic)
    );
    assert_low_level_read_result_mismatch_diagnostic(switch_unsafe_failure_path, 6, intrinsic, "Byte", "UInt32");

    auto switch_unsafe_success_path = std::filesystem::temp_directory_path() /
                                      ("orison_semantics_" + std::string(intrinsic) +
                                       "_final_switch_unsafe_block_expression_type_success.or");
    write_concurrency_fixture(
        switch_unsafe_success_path,
        "demo.unsafe",
        low_level_final_switch_unsafe_block_read_success_lines(intrinsic)
    );
    assert_fixture_success(switch_unsafe_success_path);
}

void test_low_level_read_final_container_return_types(std::string_view intrinsic) {
    auto if_failure_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_if_return_type_failure.or");
    write_concurrency_fixture(
        if_failure_path,
        "demo.unsafe",
        low_level_final_if_lines(
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "return " + low_level_read_call(intrinsic, "left"),
            "return 1 as UInt32"
        )
    );
    assert_low_level_read_result_mismatch_diagnostic(if_failure_path, 4, intrinsic, "Byte", "UInt32");

    auto if_success_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_if_return_type_success.or");
    write_concurrency_fixture(
        if_success_path,
        "demo.unsafe",
        low_level_final_if_lines(
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "return " + low_level_read_call(intrinsic, "left"),
            "return " + low_level_read_call(intrinsic, "right")
        )
    );
    assert_fixture_success(if_success_path);

    auto switch_failure_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_switch_return_type_failure.or");
    write_concurrency_fixture(
        switch_failure_path,
        "demo.unsafe",
        low_level_final_switch_lines(
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "return " + low_level_read_call(intrinsic, "left"),
            "return 1 as UInt32"
        )
    );
    assert_low_level_read_result_mismatch_diagnostic(switch_failure_path, 4, intrinsic, "Byte", "UInt32");

    auto switch_success_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_switch_return_type_success.or");
    write_concurrency_fixture(
        switch_success_path,
        "demo.unsafe",
        low_level_final_switch_lines(
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "return " + low_level_read_call(intrinsic, "left"),
            "return " + low_level_read_call(intrinsic, "right")
        )
    );
    assert_fixture_success(switch_success_path);
}

void test_low_level_read_final_ternary_expression_types(std::string_view intrinsic) {
    auto expression_failure_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_ternary_expression_type_failure.or");
    write_concurrency_fixture(
        expression_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
        }
    );
    assert_low_level_read_result_mismatch_diagnostic(expression_failure_path, 3, intrinsic, "Byte", "UInt32");

    auto expression_success_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_ternary_expression_type_success.or");
    write_concurrency_fixture(
        expression_success_path,
        "demo.unsafe",
        {
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    flag ? " + low_level_read_call(intrinsic, "left") + " : " +
                low_level_read_call(intrinsic, "right"),
        }
    );
    assert_fixture_success(expression_success_path);

    auto return_failure_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_ternary_return_type_failure.or");
    write_concurrency_fixture(
        return_failure_path,
        "demo.unsafe",
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    return flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
        }
    );
    assert_low_level_read_result_mismatch_diagnostic(return_failure_path, 3, intrinsic, "Byte", "UInt32");

    auto return_success_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_ternary_return_type_success.or");
    write_concurrency_fixture(
        return_success_path,
        "demo.unsafe",
        {
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    return flag ? " + low_level_read_call(intrinsic, "left") + " : " +
                low_level_read_call(intrinsic, "right"),
        }
    );
    assert_fixture_success(return_success_path);
}

void test_low_level_read_nested_final_container_ternary_expression_types(std::string_view intrinsic) {
    auto unsafe_failure_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_unsafe_block_final_ternary_type_failure.or");
    write_concurrency_fixture(
        unsafe_failure_path,
        "demo.unsafe",
        low_level_final_unsafe_block_ternary_read_mismatch_lines(intrinsic)
    );
    assert_low_level_read_result_mismatch_diagnostic(unsafe_failure_path, 4, intrinsic, "Byte", "UInt32");

    auto unsafe_success_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_unsafe_block_final_ternary_success.or");
    write_concurrency_fixture(
        unsafe_success_path,
        "demo.unsafe",
        low_level_final_unsafe_block_ternary_read_success_lines(intrinsic)
    );
    assert_fixture_success(unsafe_success_path);

    auto if_failure_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_if_ternary_type_failure.or");
    write_concurrency_fixture(
        if_failure_path,
        "demo.unsafe",
        low_level_final_if_ternary_read_mismatch_lines(intrinsic)
    );
    assert_low_level_read_result_mismatch_diagnostic(if_failure_path, 4, intrinsic, "Byte", "UInt32");

    auto if_success_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_if_ternary_success.or");
    write_concurrency_fixture(
        if_success_path,
        "demo.unsafe",
        low_level_final_if_ternary_read_success_lines(intrinsic)
    );
    assert_fixture_success(if_success_path);

    auto switch_failure_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_switch_ternary_type_failure.or");
    write_concurrency_fixture(
        switch_failure_path,
        "demo.unsafe",
        low_level_final_switch_ternary_read_mismatch_lines(intrinsic)
    );
    assert_low_level_read_result_mismatch_diagnostic(switch_failure_path, 4, intrinsic, "Byte", "UInt32");

    auto switch_success_path =
        std::filesystem::temp_directory_path() /
        ("orison_semantics_" + std::string(intrinsic) + "_final_switch_ternary_success.or");
    write_concurrency_fixture(
        switch_success_path,
        "demo.unsafe",
        low_level_final_switch_ternary_read_success_lines(intrinsic)
    );
    assert_fixture_success(switch_success_path);
}

void test_raw_read_guard_failure_final_expression_type_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_guard_failure_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_guard_success_lines("raw_read")
    );

    assert_fixture_success(path);
}

void test_raw_read_guard_failure_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_guard_failure_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_guard_mismatch_lines("raw_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 6, "Byte", "UInt32");
}

void test_raw_read_while_zero_iteration_final_expression_type_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_while_zero_iteration_final_expression_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_while_success_lines("raw_read")
    );

    assert_fixture_success(path);
}

void test_raw_read_for_zero_iteration_final_expression_type_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_for_zero_iteration_final_expression_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_for_success_lines("raw_read")
    );

    assert_fixture_success(path);
}

void test_raw_read_for_final_expression_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_for_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_for_mismatch_lines("raw_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 6, "Byte", "UInt32");
}

void test_raw_read_repeat_final_expression_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_repeat_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_repeat_mismatch_lines("raw_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 7, "Byte", "UInt32");
}

void test_raw_read_return_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_return_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    return raw_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_final_expression_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_direct_success_lines("raw_read")
    );

    assert_fixture_success(path);
}

void test_raw_read_rebound_final_expression_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_rebound_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_rebound_success_lines("raw_read")
    );

    assert_fixture_success(path);
}

void test_raw_read_unsafe_block_final_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_unsafe_block_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_unsafe_block_direct_success_lines("raw_read")
    );

    assert_fixture_success(path);
}

void test_raw_read_unsafe_block_rebound_final_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_unsafe_block_rebound_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_unsafe_block_rebound_success_lines("raw_read")
    );

    assert_fixture_success(path);
}

void test_raw_read_branch_merged_final_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_branch_merged_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_branch_success_lines("raw_read")
    );

    assert_fixture_success(path);
}

void test_raw_read_switch_merged_final_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_switch_merged_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_switch_success_lines("raw_read")
    );

    assert_fixture_success(path);
}

void test_raw_read_final_if_expression_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_final_if_expression_type_success.or";
    write_concurrency_fixture(path, "demo.unsafe", low_level_final_if_read_success_lines("raw_read"));

    assert_fixture_success(path);
}

void test_raw_read_final_switch_expression_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_final_switch_expression_type_success.or";
    write_concurrency_fixture(path, "demo.unsafe", low_level_final_switch_read_success_lines("raw_read"));

    assert_fixture_success(path);
}

void test_raw_read_return_same_width_integer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_read_return_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> UInt32",
            "    return raw_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_read_return_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_read_return_pointer_sized_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> IntSize",
            "    return raw_read(p)",
        }
    );

    assert_raw_read_result_mismatch_diagnostic(path, 3, "Byte", "IntSize");
}

void test_raw_write_value_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_raw_write_constant_value_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_constant_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "const STATUS_BYTE: Byte = 1",
            "unsafe function write_word(p: Pointer<UInt32>) -> Unit",
            "    raw_write(p, STATUS_BYTE)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 4, "Byte", "UInt32");
}

void test_raw_write_value_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_value_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: UInt32) -> Unit",
            "    raw_write(p, value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_same_width_integer_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_same_width_integer_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit",
            "    raw_write(p, value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_pointer_sized_integer_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_pointer_sized_integer_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit",
            "    raw_write(p, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_raw_write_computed_integer_sum_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_computed_integer_sum_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(input) + 1)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_computed_bitwise_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_computed_bitwise_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    raw_write(out, value bit_or 1)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_computed_ternary_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_computed_ternary_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    raw_write(out, flag ? left : right)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_raw_write_rebound_computed_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_rebound_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    let masked = value bit_or 1",
            "    raw_write(out, masked)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_branch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_branch_merged_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left bit_or 1",
            "    else",
            "        selected = right + 1",
            "    raw_write(out, selected)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_branch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_branch_merged_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left + 1",
            "    else",
            "        selected = right shift_left 1",
            "    raw_write(out, selected)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 8, "IntSize", "UInt32");
}

void test_raw_write_switch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_switch_merged_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left bit_or 1",
            "        default => selected = right + 1",
            "    raw_write(out, selected)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_switch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_switch_merged_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left + 1",
            "        default => selected = right shift_left 1",
            "    raw_write(out, selected)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 7, "IntSize", "UInt32");
}

void test_raw_write_array_indexed_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_array_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit",
            "    raw_write(out, items[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_bound_array_literal_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_bound_array_literal_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit",
            "    let staged = [left, right]",
            "    raw_write(out, staged[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_array_indexed_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_array_indexed_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit",
            "    raw_write(out, items[0])",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_raw_write_helper_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_helper_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function words() -> DynamicArray<Int32>",
            "    return []",
            "unsafe function write_word(out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, words()[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_member_container_field_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_container_field_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.words[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_nested_scalar_field_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_nested_scalar_field_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.registers.status)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_nested_scalar_field_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_nested_scalar_field_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.registers.status bit_or 1)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_nested_scalar_field_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_nested_scalar_field_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: IntSize",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.registers.status)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 7, "IntSize", "UInt32");
}

void test_raw_write_method_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_method_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "extend Device",
            "    function words_view(this: shared This) -> DynamicArray<Int32>",
            "        this.words",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.words_view()[0])",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_member_container_field_indexed_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_container_field_indexed_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<IntSize>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    raw_write(out, device.words[0])",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "IntSize", "UInt32");
}

void test_raw_write_integer_literal_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_integer_literal_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>) -> Unit",
            "    raw_write(p, 0)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_integer_cast_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_integer_cast_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as UInt32)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_integer_cast_target_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_integer_cast_target_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as Int64)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "Int64", "UInt32");
}

void test_raw_write_same_width_integer_cast_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_same_width_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as Int32)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_pointer_sized_integer_cast_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_pointer_sized_integer_cast_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    raw_write(p, value as IntSize)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_raw_write_recovered_raw_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_recovered_raw_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_raw_write_recovered_raw_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_recovered_raw_read_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(Pointer(address_of(buf.data[0])), 1)) as Int32)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_helper_returned_raw_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_helper_returned_raw_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(byte_ptr(base), 1)))",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_raw_write_member_returned_raw_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_returned_raw_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(device.byte_ptr(base), 1)))",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 9, "Byte", "UInt32");
}

void test_raw_write_member_returned_raw_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_returned_raw_read_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    raw_write(out, raw_read(raw_offset(device.byte_ptr(base), 1)) as UInt32)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_non_integer_cast_value_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_non_integer_cast_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Bool) -> Unit",
            "    raw_write(p, value as Bool)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 3, "Bool", "UInt32");
}

void test_raw_write_helper_pointer_constructor_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(addr: Address) -> Pointer<Byte>",
            "    return Pointer(addr)",
            "unsafe function write_word(addr: Address, value: UInt32) -> Unit",
            "    raw_write(byte_ptr(addr), value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_raw_write_helper_pointer_constructor_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_raw_write_helper_pointer_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function byte_ptr(addr: Address) -> Pointer<Byte>",
            "    return Pointer(addr)",
            "unsafe function write_byte(addr: Address, value: Byte) -> Unit",
            "    raw_write(byte_ptr(addr), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_member_helper_pointer_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function byte_ptr(this: shared This, addr: Address) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(addr)",
            "unsafe function write_word(device: Device, addr: Address, value: UInt32) -> Unit",
            "    raw_write(device.byte_ptr(addr), value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 9, "UInt32", "Byte");
}

void test_raw_write_member_helper_pointer_constructor_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_helper_pointer_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function byte_ptr(this: shared This, addr: Address) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(addr)",
            "unsafe function write_byte(device: Device, addr: Address, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(addr), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_raw_offset_helper_pointer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_raw_offset_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(next_byte_ptr(base), value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_raw_write_raw_offset_helper_pointer_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_raw_offset_helper_pointer_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_byte_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
            "unsafe function write_byte(base: Pointer<Byte>, value: Byte) -> Unit",
            "    raw_write(next_byte_ptr(base), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_member_raw_offset_helper_pointer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_member_raw_offset_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function next_byte_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, value: UInt32) -> Unit",
            "    raw_write(device.next_byte_ptr(base), value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 9, "UInt32", "Byte");
}

void test_raw_write_record_pointer_field_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_record_pointer_field_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptr: Pointer<Byte>",
            "unsafe function write_word(device: Device, value: UInt32) -> Unit",
            "    raw_write(device.ptr, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_member_field_address_inference_enables_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_member_field_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    base: Address",
            "extend Device",
            "    function byte_ptr(this: shared This) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(this.base)",
            "unsafe function write_byte(device: Device, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(), value)",
        }
    );

    assert_fixture_success(path);
}

void test_raw_write_indexed_record_pointer_field_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_raw_write_indexed_record_pointer_field_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit",
            "    raw_write(device.ptrs[index], value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 5, "UInt32", "Byte");
}

void test_indexed_member_field_address_inference_enables_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_indexed_member_field_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(this.bases[index])",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(index), value)",
        }
    );

    assert_fixture_success(path);
}

void test_rebound_indexed_record_pointer_field_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_rebound_indexed_record_pointer_field_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function write_word(device: Device, index: Int64, value: UInt32) -> Unit",
            "    let p = device.ptrs[index]",
            "    raw_write(p, value)",
        }
    );

    assert_raw_write_value_pointee_mismatch_diagnostic(path, 6, "UInt32", "Byte");
}

void test_rebound_indexed_member_field_address_inference_enables_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_rebound_indexed_member_field_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>",
            "        unsafe",
            "            let base = this.bases[index]",
            "            return Pointer(base)",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(index), value)",
        }
    );

    assert_fixture_success(path);
}

void test_return_rebound_indexed_record_pointer_field_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_record_pointer_field_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function byte_ptr(device: Device, index: Int64) -> Pointer<Byte>",
            "    let p = device.ptrs[index]",
            "    return p",
        }
    );

    assert_fixture_success(path);
}

void test_return_rebound_indexed_member_field_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_member_field_address_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function base_at(this: shared This, index: Int64) -> Address",
            "        let base = this.bases[index]",
            "        return base",
            "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(this.base_at(index))",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(index), value)",
        }
    );

    assert_fixture_success(path);
}

void test_return_rebound_indexed_pointer_used_by_helper_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_pointer_used_by_helper_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    ptrs: Pointer<Pointer<Byte>>",
            "unsafe function byte_ptr(device: Device, index: Int64) -> Pointer<Byte>",
            "    let p = device.ptrs[index]",
            "    return p",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(byte_ptr(device, index), value)",
        }
    );

    assert_fixture_success(path);
}

void test_return_rebound_indexed_address_used_by_pointer_constructor_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_return_rebound_indexed_address_pointer_constructor_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function base_at(this: shared This, index: Int64) -> Address",
            "        let base = this.bases[index]",
            "        return base",
            "    function byte_ptr(this: shared This, index: Int64) -> Pointer<Byte>",
            "        unsafe",
            "            return Pointer(this.base_at(index))",
            "unsafe function write_byte(device: Device, index: Int64, value: Byte) -> Unit",
            "    raw_write(device.byte_ptr(index), value)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_read_return_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_return_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    return volatile_read(p)",
        }
    );

    assert_volatile_read_result_mismatch_diagnostic(path, 3, "Byte", "Pointer<Byte>");
}

void test_volatile_read_final_expression_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_direct_mismatch_lines("volatile_read")
    );

    assert_volatile_read_result_mismatch_diagnostic(path, 3, "Byte", "Pointer<Byte>");
}

void test_volatile_read_rebound_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_rebound_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_rebound_mismatch_lines("volatile_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 4, "Byte", "UInt32");
}

void test_volatile_read_unsafe_block_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_unsafe_block_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_unsafe_block_direct_mismatch_lines("volatile_read")
    );

    assert_volatile_read_result_mismatch_diagnostic(path, 4, "Byte", "Pointer<Byte>");
}

void test_volatile_read_unsafe_block_rebound_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_unsafe_block_rebound_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_unsafe_block_rebound_mismatch_lines("volatile_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_volatile_read_branch_merged_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_branch_merged_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_branch_mismatch_lines("volatile_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 8, "Byte", "UInt32");
}

void test_volatile_read_switch_merged_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_switch_merged_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_switch_mismatch_lines("volatile_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 7, "Byte", "UInt32");
}

void test_volatile_read_final_if_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_final_if_expression_type_failure.or";
    write_concurrency_fixture(path, "demo.unsafe", low_level_final_if_read_mismatch_lines("volatile_read"));

    assert_volatile_read_result_mismatch_diagnostic(path, 4, "Byte", "UInt32");
}

void test_volatile_read_final_switch_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_final_switch_expression_type_failure.or";
    write_concurrency_fixture(path, "demo.unsafe", low_level_final_switch_read_mismatch_lines("volatile_read"));

    assert_volatile_read_result_mismatch_diagnostic(path, 4, "Byte", "UInt32");
}

void test_volatile_read_guard_failure_final_expression_type_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_guard_failure_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_guard_success_lines("volatile_read")
    );

    assert_fixture_success(path);
}

void test_volatile_read_guard_failure_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_guard_failure_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_guard_mismatch_lines("volatile_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 6, "Byte", "UInt32");
}

void test_volatile_read_while_zero_iteration_final_expression_type_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_while_zero_iteration_final_expression_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_while_success_lines("volatile_read")
    );

    assert_fixture_success(path);
}

void test_volatile_read_for_zero_iteration_final_expression_type_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_for_zero_iteration_final_expression_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_for_success_lines("volatile_read")
    );

    assert_fixture_success(path);
}

void test_volatile_read_for_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_for_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_for_mismatch_lines("volatile_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 6, "Byte", "UInt32");
}

void test_volatile_read_repeat_final_expression_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_repeat_final_expression_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_repeat_mismatch_lines("volatile_read")
    );

    assert_final_expression_type_mismatch_diagnostic(path, 7, "Byte", "UInt32");
}

void test_volatile_read_return_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_return_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    return volatile_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_read_final_expression_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_direct_success_lines("volatile_read")
    );

    assert_fixture_success(path);
}

void test_volatile_read_rebound_final_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_rebound_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_rebound_success_lines("volatile_read")
    );

    assert_fixture_success(path);
}

void test_volatile_read_unsafe_block_final_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_unsafe_block_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_unsafe_block_direct_success_lines("volatile_read")
    );

    assert_fixture_success(path);
}

void test_volatile_read_unsafe_block_rebound_final_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_unsafe_block_rebound_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_unsafe_block_rebound_success_lines("volatile_read")
    );

    assert_fixture_success(path);
}

void test_volatile_read_branch_merged_final_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_branch_merged_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_branch_success_lines("volatile_read")
    );

    assert_fixture_success(path);
}

void test_volatile_read_switch_merged_final_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_switch_merged_final_expression_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        low_level_final_read_switch_success_lines("volatile_read")
    );

    assert_fixture_success(path);
}

void test_volatile_read_final_if_expression_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_final_if_expression_type_success.or";
    write_concurrency_fixture(path, "demo.unsafe", low_level_final_if_read_success_lines("volatile_read"));

    assert_fixture_success(path);
}

void test_volatile_read_final_switch_expression_type_match_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_final_switch_expression_type_success.or";
    write_concurrency_fixture(path, "demo.unsafe", low_level_final_switch_read_success_lines("volatile_read"));

    assert_fixture_success(path);
}

void test_volatile_read_return_same_width_integer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_return_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> UInt32",
            "    return volatile_read(p)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_read_return_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_return_pointer_sized_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> IntSize",
            "    return volatile_read(p)",
        }
    );

    assert_volatile_read_result_mismatch_diagnostic(path, 3, "Byte", "IntSize");
}

void test_volatile_read_typed_binding_result_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_typed_binding_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> UInt32",
            "    let value: UInt32 = volatile_read(p)",
            "    return value",
        }
    );

    assert_volatile_read_binding_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_volatile_read_typed_binding_result_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_read_typed_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    let value: Byte = volatile_read(p)",
            "    return value",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_read_typed_binding_same_width_integer_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_typed_binding_same_width_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Int32>) -> Int32",
            "    let value: UInt32 = volatile_read(p)",
            "    return value as Int32",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_read_typed_binding_pointer_sized_integer_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_read_typed_binding_pointer_sized_mismatch.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function read_word(p: Pointer<Byte>) -> IntSize",
            "    let value: IntSize = volatile_read(p)",
            "    return value",
        }
    );

    assert_volatile_read_binding_mismatch_diagnostic(path, 3, "Byte", "IntSize");
}

void test_volatile_write_value_type_mismatch_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "Byte", "UInt32");
}

void test_volatile_write_value_type_match_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_value_type_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: UInt32) -> Unit",
            "    volatile_write(p, value)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_same_width_integer_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_same_width_integer_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Int32) -> Unit",
            "    volatile_write(p, value)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_pointer_sized_integer_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_pointer_sized_integer_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: IntSize) -> Unit",
            "    volatile_write(p, value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_volatile_write_computed_integer_sum_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_computed_integer_sum_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(input: Pointer<Int32>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(input) + 1)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_computed_bitwise_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_computed_bitwise_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    volatile_write(out, value bit_or 1)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_computed_ternary_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_computed_ternary_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    volatile_write(out, flag ? left : right)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_volatile_write_rebound_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_rebound_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, value: Int32) -> Unit",
            "    let masked = value bit_or 1",
            "    volatile_write(out, masked)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_branch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_branch_merged_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left bit_or 1",
            "    else",
            "        selected = right + 1",
            "    volatile_write(out, selected)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_branch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_branch_merged_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, flag: Bool, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    if flag",
            "        selected = left + 1",
            "    else",
            "        selected = right shift_left 1",
            "    volatile_write(out, selected)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 8, "IntSize", "UInt32");
}

void test_volatile_write_switch_merged_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_switch_merged_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: Int32, right: Int32) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left bit_or 1",
            "        default => selected = right + 1",
            "    volatile_write(out, selected)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_switch_merged_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_switch_merged_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, selector: UInt32, left: IntSize, right: IntSize) -> Unit",
            "    var selected = left",
            "    switch selector",
            "        0 => selected = left + 1",
            "        default => selected = right shift_left 1",
            "    volatile_write(out, selected)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 7, "IntSize", "UInt32");
}

void test_volatile_write_array_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_array_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<Int32>) -> Unit",
            "    volatile_write(out, items[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_bound_array_literal_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_bound_array_literal_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, left: Int32, right: Int32) -> Unit",
            "    let staged = [left, right]",
            "    volatile_write(out, staged[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_array_indexed_pointer_sized_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_array_indexed_pointer_sized_value_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(out: Pointer<UInt32>, items: DynamicArray<IntSize>) -> Unit",
            "    volatile_write(out, items[0])",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_volatile_write_helper_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_helper_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function words() -> DynamicArray<Int32>",
            "    return []",
            "unsafe function write_word(out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, words()[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_member_container_field_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_container_field_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.words[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_nested_scalar_field_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_nested_scalar_field_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.registers.status)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_nested_scalar_field_computed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_nested_scalar_field_computed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: Int32",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.registers.status bit_or 1)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_nested_scalar_field_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_nested_scalar_field_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Registers",
            "    status: IntSize",
            "record Device",
            "    registers: Registers",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.registers.status)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 7, "IntSize", "UInt32");
}

void test_volatile_write_method_returned_container_indexed_value_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_method_returned_container_indexed_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<Int32>",
            "extend Device",
            "    function words_view(this: shared This) -> DynamicArray<Int32>",
            "        this.words",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.words_view()[0])",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_member_container_field_indexed_pointer_sized_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_container_field_indexed_pointer_sized_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    words: DynamicArray<IntSize>",
            "unsafe function write_word(out: Pointer<UInt32>, device: Device) -> Unit",
            "    volatile_write(out, device.words[0])",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "IntSize", "UInt32");
}

void test_volatile_write_integer_literal_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_integer_literal_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>) -> Unit",
            "    volatile_write(p, 0)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_integer_cast_value_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_volatile_write_integer_cast_value_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as UInt32)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_integer_cast_target_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_integer_cast_target_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as Int64)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "Int64", "UInt32");
}

void test_volatile_write_same_width_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_same_width_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as Int32)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_pointer_sized_integer_cast_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_pointer_sized_integer_cast_mismatch_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(p, value as IntSize)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "IntSize", "UInt32");
}

void test_volatile_write_recovered_volatile_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_recovered_volatile_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(Pointer(address_of(buf.data[0])), 1)))",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_volatile_write_recovered_volatile_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_recovered_volatile_read_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function write_word(buf: Buffer, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(Pointer(address_of(buf.data[0])), 1)) as Int32)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_helper_returned_volatile_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_helper_returned_volatile_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function word_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(word_ptr(base), 1)))",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_volatile_write_member_returned_volatile_read_value_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_returned_volatile_read_value_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function word_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(device.word_ptr(base), 1)))",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 9, "Byte", "UInt32");
}

void test_volatile_write_member_returned_volatile_read_integer_cast_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_returned_volatile_read_integer_cast_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function word_ptr(this: shared This, base: Pointer<Byte>) -> Pointer<Byte>",
            "        unsafe",
            "            return raw_offset(base, 1)",
            "unsafe function write_word(device: Device, base: Pointer<Byte>, out: Pointer<UInt32>) -> Unit",
            "    volatile_write(out, volatile_read(raw_offset(device.word_ptr(base), 1)) as UInt32)",
        }
    );

    assert_fixture_success(path);
}

void test_volatile_write_non_integer_cast_value_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_non_integer_cast_value_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function write_word(p: Pointer<UInt32>, value: Bool) -> Unit",
            "    volatile_write(p, value as Bool)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 3, "Bool", "UInt32");
}

void test_volatile_write_helper_pointer_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function word_ptr(addr: Address) -> Pointer<UInt32>",
            "    return Pointer(addr)",
            "unsafe function write_word(addr: Address, value: Byte) -> Unit",
            "    volatile_write(word_ptr(addr), value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_volatile_write_member_helper_pointer_constructor_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_member_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    id: Int64",
            "extend Device",
            "    function word_ptr(this: shared This, addr: Address) -> Pointer<UInt32>",
            "        unsafe",
            "            return Pointer(addr)",
            "unsafe function write_word(device: Device, addr: Address, value: Byte) -> Unit",
            "    volatile_write(device.word_ptr(addr), value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 9, "Byte", "UInt32");
}

void test_volatile_write_raw_offset_helper_pointer_type_mismatch_failure() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_volatile_write_raw_offset_helper_pointer_type_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function next_word_ptr(base: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return raw_offset(base, 1)",
            "unsafe function write_word(base: Pointer<UInt32>, value: Byte) -> Unit",
            "    volatile_write(next_word_ptr(base), value)",
        }
    );

    assert_volatile_write_value_pointee_mismatch_diagnostic(path, 5, "Byte", "UInt32");
}

void test_address_typed_binding_with_nonaddress_initializer_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_typed_binding_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function read_base() -> Address",
            "    let base: Address = \"text\"",
            "    return 0x4000_1000",
        }
    );

    assert_address_binding_initializer_diagnostic(path, 3);
}

void test_address_typed_binding_with_address_initializer_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_typed_binding_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function first_addr(buf: exclusive Buffer) -> Address",
            "    let base: Address = address_of(buf.data[0])",
            "    return base",
        }
    );

    assert_fixture_success(path);
}

void test_address_typed_binding_with_field_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_typed_binding_field_address_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    base: Address",
            "function read_base(device: Device) -> Address",
            "    let base: Address = device.base",
            "    return base",
        }
    );

    assert_fixture_success(path);
}

void test_address_typed_binding_with_indexed_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_typed_binding_indexed_address_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "function read_base(device: Device, index: Int64) -> Address",
            "    let base: Address = device.bases[index]",
            "    return base",
        }
    );

    assert_fixture_success(path);
}

void test_address_typed_binding_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_typed_binding_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function read_base() -> Address",
            "    let source = \"text\"",
            "    let base: Address = source",
            "    return 0x4000_1000",
        }
    );

    assert_address_binding_initializer_diagnostic(path, 4);
}

void test_address_return_with_nonaddress_expression_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function base() -> Address",
            "    return \"text\"",
        }
    );

    assert_address_return_diagnostic(path, 3);
}

void test_address_final_expression_with_nonaddress_expression_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_final_expression_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function base() -> Address",
            "    \"text\"",
        }
    );

    assert_address_return_diagnostic(path, 3);
}

void test_address_return_with_address_expression_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function base(buf: exclusive Buffer) -> Address",
            "    return address_of(buf.data[0])",
        }
    );

    assert_fixture_success(path);
}

void test_address_final_expression_with_address_expression_success() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_final_expression_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "unsafe function base(buf: exclusive Buffer) -> Address",
            "    address_of(buf.data[0])",
        }
    );

    assert_fixture_success(path);
}

void test_address_return_ternary_expression_types() {
    auto mismatch_path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_ternary_failure.or";
    write_concurrency_fixture(
        mismatch_path,
        "demo.unsafe",
        {
            "unsafe function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    return flag ? address_of(buf.data[0]) : \"text\"",
        }
    );
    assert_address_return_diagnostic(mismatch_path, 3);

    auto success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_ternary_success.or";
    write_concurrency_fixture(
        success_path,
        "demo.unsafe",
        {
            "unsafe function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    return flag ? address_of(buf.data[0]) : address_of(buf.data[1])",
        }
    );
    assert_fixture_success(success_path);
}

void test_address_nested_final_container_ternary_expression_types() {
    auto unsafe_success_path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_unsafe_final_ternary_success.or";
    write_concurrency_fixture(
        unsafe_success_path,
        "demo.unsafe",
        {
            "function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    unsafe",
            "        flag ? address_of(buf.data[0]) : address_of(buf.data[1])",
        }
    );
    assert_fixture_success(unsafe_success_path);

    auto if_mismatch_path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_final_if_ternary_failure.or";
    write_concurrency_fixture(
        if_mismatch_path,
        "demo.unsafe",
        {
            "unsafe function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    if flag",
            "        flag ? address_of(buf.data[0]) : \"text\"",
            "    else",
            "        address_of(buf.data[1])",
        }
    );
    assert_address_return_diagnostic(if_mismatch_path, 4);
}

void test_address_return_with_helper_returned_address_success() {
    auto path = std::filesystem::temp_directory_path() /
                "orison_semantics_address_return_helper_returned_address_success.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "record Device",
            "    bases: Pointer<Address>",
            "extend Device",
            "    function base_at(this: shared This, index: Int64) -> Address",
            "        let base = this.bases[index]",
            "        return base",
            "function read_base(device: Device, index: Int64) -> Address",
            "    return device.base_at(index)",
        }
    );

    assert_fixture_success(path);
}

void test_address_return_with_wrong_typed_name_failure() {
    auto path =
        std::filesystem::temp_directory_path() / "orison_semantics_address_return_name_failure.or";
    write_concurrency_fixture(
        path,
        "demo.unsafe",
        {
            "function base() -> Address",
            "    let source = \"text\"",
            "    return source",
        }
    );

    assert_address_return_diagnostic(path, 4);
}

void test_task_outside_async_function_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_sync_failure.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        request(url)",
            "    return request(url)",
        }
    );

    assert_task_inside_async_diagnostic(path, 3);
}

void test_thread_outside_async_function_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_sync_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        sum(data)",
            "    return worker.join()",
        }
    );

    assert_fixture_success(path);
}

void test_thread_join_receiver_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_join_receiver_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum() -> Int64",
            "    let worker = thread",
            "        1",
            "    return worker.join()",
        }
    );

    assert_fixture_success(path);
}

void test_join_non_thread_receiver_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_join_non_thread_receiver_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "async function fetch() -> Int64",
            "    let request_task = task",
            "        1",
            "    return request_task.join()",
        }
    );

    assert_join_task_receiver_diagnostic(path, 5);
}

void test_join_async_call_receiver_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_join_async_call_receiver_failure.or";
    write_concurrency_fixture(
        path,
        "demo.await",
        {
            "async function request(url: Text) -> Outcome<Text, IOError>",
            "    return fetch_remote(url)",
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let pending = request(url)",
            "    return pending.join()",
        }
    );

    assert_join_async_call_receiver_diagnostic(path, 6);
}

void test_thread_value_without_join_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_value_without_join_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum() -> Int64",
            "    let worker = thread",
            "        1",
            "    return worker",
        }
    );

    assert_thread_return_forward_diagnostic(path, 5);
}

void test_concurrency_capture_classification_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_capture_classification_success.or";
    write_concurrency_fixture(
        path,
        "demo.capture",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let cached = url",
            "    let request_task = task",
            "        cached",
            "    return await request_task",
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        sum(data)",
            "    return worker.join()",
        }
    );

    auto analysis = analyze_orison_fixture(path);
    assert(!analysis.has_errors());
    assert(analysis.concurrency_captures.size() == 2);
    assert(analysis.concurrency_captures[0].line == 5);
    assert(analysis.concurrency_captures[0].name == "cached");
    assert(analysis.concurrency_captures[0].type_name == "Text");
    assert(analysis.concurrency_captures[0].expression_kind ==
           orison::semantics::ConcurrencyExpressionKind::task);
    assert(analysis.concurrency_captures[0].capture_kind ==
           orison::semantics::ConcurrencyCaptureKind::immutable_outer_local);
    assert(analysis.concurrency_captures[1].line == 9);
    assert(analysis.concurrency_captures[1].name == "data");
    assert(analysis.concurrency_captures[1].type_name == "shared.View<Int64>");
    assert(analysis.concurrency_captures[1].expression_kind ==
           orison::semantics::ConcurrencyExpressionKind::thread);
    assert(analysis.concurrency_captures[1].capture_kind ==
           orison::semantics::ConcurrencyCaptureKind::parameter);
}

void test_thread_capture_owned_parameter_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_owned_parameter_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch_processing(buffer: Buffer) -> Int64",
            "    let worker = thread",
            "        process(buffer)",
            "    return worker.join()",
        }
    );

    auto analysis = analyze_orison_fixture(path);
    assert_thread_capture_transferable_diagnostic(analysis, 4, "buffer", "Buffer");
    assert(analysis.concurrency_captures.size() == 1);
    assert_concurrency_capture(
        analysis,
        0,
        "buffer",
        "Buffer",
        orison::semantics::ConcurrencyExpressionKind::thread,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_thread_capture_transferable_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_transferable_generic_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch<T>(item: T) -> Int64",
            "where T: Transferable",
            "    let worker = thread",
            "        process(item)",
            "    return worker.join()",
        }
    );

    assert_single_concurrency_capture_success(
        path,
        "item",
        "T",
        orison::semantics::ConcurrencyExpressionKind::thread,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_thread_capture_unconstrained_generic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_unconstrained_generic_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch<T>(item: T) -> Int64",
            "    let worker = thread",
            "        process(item)",
            "    return worker.join()",
        }
    );

    assert_thread_capture_transferable_diagnostic(path, 4, "item", "T");
}

void test_thread_capture_transferable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_transferable_concrete_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "implements Transferable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing(buffer: Buffer) -> Int64",
            "    let worker = thread",
            "        process(buffer)",
            "    return worker.join()",
        }
    );

    assert_single_concurrency_capture_success(
        path,
        "buffer",
        "Buffer",
        orison::semantics::ConcurrencyExpressionKind::thread,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_thread_capture_shareable_generic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_shareable_generic_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch<T>(item: T) -> Int64",
            "where T: Shareable",
            "    let worker = thread",
            "        process(item)",
            "    return worker.join()",
        }
    );

    assert_thread_capture_transferable_diagnostic(path, 5, "item", "T");
}

void test_task_capture_shareable_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_shareable_generic_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function launch<T>(item: T) -> T",
            "where T: Shareable",
            "    let worker = task",
            "        item",
            "    return await worker",
        }
    );

    assert_single_concurrency_capture_success(
        path,
        "item",
        "T",
        orison::semantics::ConcurrencyExpressionKind::task,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_task_capture_shareable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_shareable_concrete_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "async function launch_processing(buffer: Buffer) -> Buffer",
            "    let worker = task",
            "        buffer",
            "    return await worker",
        }
    );

    assert_single_concurrency_capture_success(
        path,
        "buffer",
        "Buffer",
        orison::semantics::ConcurrencyExpressionKind::task,
        orison::semantics::ConcurrencyCaptureKind::parameter
    );
}

void test_thread_capture_shareable_concrete_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_shareable_concrete_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing(buffer: Buffer) -> Int64",
            "    let worker = thread",
            "        process(buffer)",
            "    return worker.join()",
        }
    );

    assert_thread_capture_transferable_diagnostic(path, 7, "buffer", "Buffer");
}

void test_thread_result_owned_concrete_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_owned_concrete_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch_processing() -> Int64",
            "    let worker = thread",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return worker.join()",
        }
    );

    assert_thread_result_transferable_diagnostic(path, 5, "Buffer");
}

void test_thread_result_transferable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_transferable_concrete_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "implements Transferable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing() -> Int64",
            "    let worker = thread",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return worker.join()",
        }
    );

    assert_thread_result_transferable_success(path);
}

void test_thread_result_shareable_generic_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_shareable_generic_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function launch<T>() -> Int64",
            "where T: Shareable",
            "    let worker = thread",
            "        let produced: T = build()",
            "        produced",
            "    return worker.join()",
        }
    );

    assert_thread_result_transferable_diagnostic(path, 6, "T");
}

void test_task_result_shareable_generic_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_result_shareable_generic_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function launch<T>() -> T",
            "where T: Shareable",
            "    let worker = task",
            "        let produced: T = build()",
            "        produced",
            "    return await worker",
        }
    );

    assert_task_result_shareable_success(path);
}

void test_task_result_shareable_concrete_type_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_result_shareable_concrete_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "async function launch_processing() -> Buffer",
            "    let worker = task",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return await worker",
        }
    );

    assert_task_result_shareable_success(path);
}

void test_thread_result_shareable_concrete_type_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_result_shareable_concrete_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "implements Shareable for Buffer",
            "    function placeholder(this: shared This) -> Unit",
            "        return",
            "function launch_processing() -> Int64",
            "    let worker = thread",
            "        let produced: Buffer = make_buffer()",
            "        produced",
            "    return worker.join()",
        }
    );

    assert_thread_result_transferable_diagnostic(path, 8, "Buffer");
}

void test_task_expression_value_boundary_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_value_boundary_failure.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        let response = request(url)",
            "    return await request_task",
        }
    );

    assert_task_value_boundary_diagnostic(path, 4);
}

void test_task_expression_value_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_value_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    let request_task = task",
            "        return request(url)",
            "    return await request_task",
        }
    );

    assert_task_value_return_success(path);
}

void test_thread_expression_value_boundary_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_value_boundary_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        let total = sum(data)",
            "    return worker.join()",
        }
    );

    assert_thread_value_boundary_diagnostic(path, 4);
}

void test_thread_expression_value_return_success() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_value_return_success.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    let worker = thread",
            "        return sum(data)",
            "    return worker.join()",
        }
    );

    assert_thread_value_return_success(path);
}

void test_task_capture_mutable_outer_local_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_task_capture_mutable_failure.or";
    write_concurrency_fixture(
        path,
        "demo.task",
        {
            "async function fetch(url: Text) -> Outcome<Text, IOError>",
            "    var attempts = 0",
            "    let request_task = task",
            "        attempts",
            "    return await request_task",
        }
    );

    assert_mutable_capture_diagnostic(path, 5, "attempts");
}

void test_thread_capture_mutable_outer_local_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_capture_mutable_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "function parallel_sum(data: shared View<Int64>) -> Int64",
            "    var total = 0",
            "    let worker = thread",
            "        total",
            "    return worker.join()",
        }
    );

    assert_mutable_capture_diagnostic(path, 5, "total");
}

void test_thread_capture_receiver_this_failure() {
    auto path = std::filesystem::temp_directory_path() / "orison_semantics_thread_capture_this_failure.or";
    write_concurrency_fixture(
        path,
        "demo.thread",
        {
            "extend Worker",
            "    function spawn(this: shared This) -> Int64",
            "        let worker = thread",
            "            this.id",
            "        return worker.join()",
        }
    );

    assert_receiver_capture_diagnostic(path, 5);
}

}  // namespace

int main() {
    test_await_inside_async_function_success();
    test_await_async_call_value_success();
    test_await_outside_async_function_failure();
    test_await_outside_async_method_failure();
    test_await_plain_value_failure();
    test_await_thread_value_failure();
    test_await_non_async_call_value_failure();
    test_await_member_call_not_marked_async_from_top_level_name_collision_failure();
    test_task_inside_async_function_success();
    test_return_task_value_failure();
    test_return_async_call_value_failure();
    test_return_thread_value_failure();
    test_assignment_preserves_async_call_origin_success();
    test_assignment_preserves_thread_origin_failure();
    test_ternary_preserves_async_call_origin_success();
    test_ternary_preserves_thread_origin_failure();
    test_return_ternary_async_origin_failure();
    test_if_branch_preserves_async_call_origin_success();
    test_if_branch_preserves_thread_origin_failure();
    test_switch_branch_preserves_async_call_origin_success();
    test_switch_branch_preserves_thread_origin_failure();
    test_while_loop_preserves_async_call_origin_success();
    test_while_loop_preserves_thread_origin_failure();
    test_repeat_loop_preserves_async_call_origin_success();
    test_repeat_loop_preserves_thread_origin_failure();
    test_for_loop_preserves_async_call_origin_success();
    test_for_loop_preserves_thread_origin_failure();
    test_guard_failure_path_does_not_override_async_origin_success();
    test_guard_failure_path_does_not_create_async_origin_failure();
    test_switch_constructor_pattern_binds_case_local_names_success();
    test_switch_top_level_name_pattern_rejects_unknown_variant_failure();
    test_switch_call_pattern_rejects_unknown_variant_failure();
    test_switch_unknown_constructor_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_nested_constructor_pattern_binds_nested_names_success();
    test_switch_nested_constructor_pattern_binds_wrapped_payload_type_for_low_level_success();
    test_switch_rejects_nested_payload_constructor_overlap_failure();
    test_switch_rejects_nested_literal_payload_constructor_overlap_failure();
    test_switch_accepts_disjoint_nested_literal_payload_constructor_patterns_success();
    test_switch_rejects_nested_wildcard_literal_payload_constructor_overlap_failure();
    test_switch_rejects_nested_literal_wildcard_payload_constructor_overlap_failure();
    test_switch_rejects_nested_multi_payload_constructor_overlap_failure();
    test_switch_accepts_disjoint_nested_multi_payload_constructor_patterns_success();
    test_switch_accepts_mismatched_nested_constructor_patterns_success();
    test_switch_rejects_duplicate_nested_zero_payload_constructor_failure();
    test_switch_rejects_duplicate_nested_zero_payload_constructor_no_cascade_failure();
    test_switch_rejects_deep_nested_payload_constructor_overlap_failure();
    test_switch_accepts_disjoint_deep_nested_literal_payload_constructor_patterns_success();
    test_switch_rejects_deep_nested_wildcard_literal_payload_constructor_overlap_failure();
    test_switch_rejects_deep_nested_literal_wildcard_payload_constructor_overlap_failure();
    test_switch_accepts_mismatched_deep_nested_zero_payload_constructor_patterns_success();
    test_switch_rejects_duplicate_deep_nested_zero_payload_constructor_failure();
    test_switch_nested_constructor_pattern_binds_wrapped_payload_type_for_low_level_failure();
    test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_success();
    test_switch_generic_constructor_pattern_binds_payload_type_for_low_level_failure();
    test_switch_constructor_pattern_rejects_variant_from_different_choice_failure();
    test_switch_wrong_choice_constructor_without_default_does_not_cascade_failure();
    test_switch_constructor_pattern_uses_subject_specific_arity_success();
    test_switch_nested_constructor_pattern_rejects_variant_from_different_payload_choice_failure();
    test_switch_nested_constructor_pattern_uses_payload_specific_arity_success();
    test_switch_nested_constructor_pattern_rejects_invalid_payload_shape_failure();
    test_switch_constructor_payload_shape_without_default_does_not_cascade_failure();
    test_switch_constructor_pattern_rejects_duplicate_binding_names_failure();
    test_switch_constructor_duplicate_binding_without_default_does_not_cascade_failure();
    test_switch_nested_constructor_pattern_rejects_duplicate_binding_names_failure();
    test_switch_nested_constructor_duplicate_binding_without_default_does_not_cascade_failure();
    test_switch_constructor_pattern_rejects_missing_payload_values_failure();
    test_switch_constructor_pattern_rejects_extra_payload_values_failure();
    test_switch_constructor_pattern_arity_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_zero_payload_constructor_arity_without_default_does_not_cascade_failure();
    test_switch_rejects_constructor_then_value_pattern_mix_failure();
    test_switch_rejects_value_then_constructor_pattern_mix_failure();
    test_switch_pattern_mix_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_rejects_mismatched_value_pattern_type_failure();
    test_switch_accepts_same_width_integer_cast_value_pattern_success();
    test_switch_rejects_duplicate_boolean_value_pattern_failure();
    test_switch_duplicate_bool_without_default_does_not_cascade_to_missing_pattern_failure();
    test_switch_rejects_duplicate_string_value_pattern_failure();
    test_switch_rejects_duplicate_integer_cast_value_pattern_failure();
    test_switch_rejects_redundant_bool_default_failure();
    test_switch_duplicate_bool_suppresses_redundant_default_failure();
    test_switch_accepts_exhaustive_bool_without_default_success();
    test_switch_rejects_missing_bool_value_pattern_failure();
    test_switch_rejects_redundant_zero_payload_choice_default_failure();
    test_switch_duplicate_zero_payload_choice_suppresses_redundant_default_failure();
    test_switch_accepts_exhaustive_zero_payload_choice_without_default_success();
    test_switch_rejects_redundant_payload_choice_default_after_full_cover_failure();
    test_switch_accepts_exhaustive_payload_choice_without_default_success();
    test_switch_accepts_reversed_exhaustive_payload_choice_without_default_success();
    test_switch_accepts_literal_payload_choice_arm_with_default_success();
    test_switch_rejects_literal_payload_choice_arm_without_default_failure();
    test_switch_rejects_reversed_literal_payload_choice_arm_without_default_failure();
    test_switch_accepts_nested_payload_choice_arm_with_default_success();
    test_switch_rejects_nested_payload_choice_arm_without_default_failure();
    test_switch_accepts_partial_multi_payload_choice_arm_with_default_success();
    test_switch_rejects_partial_multi_payload_choice_arm_without_default_failure();
    test_switch_rejects_missing_payload_choice_variant_without_default_failure();
    test_switch_accepts_exhaustive_multi_payload_choice_without_default_success();
    test_switch_rejects_redundant_multi_payload_choice_default_failure();
    test_switch_duplicate_payload_choice_suppresses_redundant_default_failure();
    test_switch_rejects_first_missing_multi_payload_choice_variant_failure();
    test_switch_rejects_second_missing_multi_payload_choice_variant_failure();
    test_switch_duplicate_multi_payload_choice_without_default_does_not_cascade_failure();
    test_switch_duplicate_payload_choice_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_rejects_duplicate_zero_payload_choice_constructor_failure();
    test_switch_duplicate_choice_without_default_does_not_cascade_to_missing_variant_failure();
    test_switch_rejects_duplicate_name_only_payload_choice_constructor_failure();
    test_switch_duplicate_payload_choice_constructor_does_not_cascade_to_binding_failure();
    test_switch_rejects_duplicate_literal_payload_choice_constructor_failure();
    test_switch_rejects_equivalent_integer_literal_payload_choice_constructor_failure();
    test_switch_rejects_wildcard_then_literal_payload_choice_constructor_failure();
    test_switch_rejects_literal_then_wildcard_payload_choice_constructor_failure();
    test_switch_rejects_multi_payload_partial_overlap_choice_constructor_failure();
    test_switch_accepts_multi_payload_disjoint_literal_choice_constructor_success();
    test_switch_accepts_multi_payload_disjoint_leading_literal_choice_constructor_success();
    test_switch_rejects_missing_zero_payload_choice_variant_failure();
    test_switch_rejects_multiple_default_cases_semantically();
    test_switch_rejects_nonfinal_default_case_semantically();
    test_switch_nonfinal_default_suppresses_branch_analysis_failure();
    test_switch_multiple_default_suppresses_branch_analysis_failure();
    test_break_outside_loop_failure();
    test_continue_outside_loop_failure();
    test_break_inside_loop_success();
    test_continue_inside_loop_success();
    test_this_outside_method_failure();
    test_receiver_parameter_outside_method_failure();
    test_qualified_this_type_in_ordinary_function_signature_failure();
    test_qualified_this_type_in_local_annotation_failure();
    test_receiver_parameter_with_nonself_type_inside_method_failure();
    test_unsafe_intrinsic_outside_unsafe_context_failure();
    test_unsafe_intrinsic_inside_unsafe_function_success();
    test_unsafe_intrinsic_inside_unsafe_block_success();
    test_address_of_nonstorage_operand_failure();
    test_raw_read_nonaddress_operand_failure();
    test_raw_offset_nonaddress_base_failure();
    test_raw_offset_noninteger_offset_failure();
    test_volatile_read_nonaddress_operand_failure();
    test_address_constant_enables_volatile_read_success();
    test_integer_literal_constant_initializer_success();
    test_constant_initializer_type_mismatch_failure();
    test_array_literal_constant_initializer_success();
    test_array_literal_constant_initializer_element_type_failure();
    test_array_literal_constant_initializer_length_failure();
    test_nested_array_literal_constant_initializer_success();
    test_indexed_array_constant_initializer_success();
    test_indexed_array_constant_initializer_element_type_failure();
    test_record_constructor_constant_initializer_field_access_success();
    test_record_constructor_constant_initializer_indexed_field_success();
    test_record_constructor_constant_initializer_arity_failure();
    test_record_constructor_constant_initializer_field_type_failure();
    test_record_constructor_let_binding_field_access_success();
    test_record_constructor_let_binding_field_type_failure();
    test_record_constructor_return_expression_success();
    test_record_constructor_return_expression_arity_failure();
    test_generic_record_constructor_repeated_field_success();
    test_generic_record_constructor_repeated_field_conflict_failure();
    test_record_constructor_choice_ternary_field_payload_failure();
    test_record_constructor_choice_ternary_field_success();
    test_record_constructor_pointer_ternary_field_failure();
    test_record_constructor_pointer_ternary_field_success();
    test_nested_record_constructor_choice_ternary_field_payload_failure();
    test_nested_record_constructor_choice_ternary_field_success();
    test_nested_record_constructor_pointer_ternary_field_failure();
    test_nested_record_constructor_pointer_ternary_field_success();
    test_choice_payload_record_constructor_choice_ternary_field_failure();
    test_choice_payload_record_constructor_choice_ternary_field_success();
    test_array_record_constructor_choice_ternary_field_failure();
    test_array_record_constructor_choice_ternary_field_success();
    test_choice_payload_record_constructor_pointer_ternary_field_failure();
    test_choice_payload_record_constructor_pointer_ternary_field_success();
    test_array_record_constructor_pointer_ternary_field_failure();
    test_array_record_constructor_pointer_ternary_field_success();
    test_nested_array_record_constructor_choice_ternary_field_failure();
    test_nested_array_record_constructor_choice_ternary_field_success();
    test_nested_array_record_constructor_pointer_ternary_field_failure();
    test_nested_array_record_constructor_pointer_ternary_field_success();
    test_record_field_nested_array_record_constructor_choice_ternary_field_failure();
    test_record_field_nested_array_record_constructor_choice_ternary_field_success();
    test_record_field_nested_array_record_constructor_pointer_ternary_field_failure();
    test_record_field_nested_array_record_constructor_pointer_ternary_field_success();
    test_record_field_nested_array_choice_constructor_ternary_payload_failure();
    test_record_field_nested_array_choice_constructor_ternary_payload_success();
    test_call_argument_nested_array_record_constructor_choice_ternary_field_failure();
    test_call_argument_nested_array_record_constructor_choice_ternary_field_success();
    test_call_argument_nested_array_choice_constructor_ternary_payload_failure();
    test_call_argument_nested_array_choice_constructor_ternary_payload_success();
    test_method_argument_nested_array_record_constructor_choice_ternary_field_failure();
    test_method_argument_nested_array_record_constructor_choice_ternary_field_success();
    test_method_argument_nested_array_choice_constructor_ternary_payload_failure();
    test_method_argument_nested_array_choice_constructor_ternary_payload_success();
    test_choice_payload_array_record_constructor_choice_ternary_field_failure();
    test_choice_payload_array_record_constructor_choice_ternary_field_success();
    test_choice_payload_array_record_constructor_pointer_ternary_field_failure();
    test_choice_payload_array_record_constructor_pointer_ternary_field_success();
    test_choice_payload_nested_array_record_constructor_choice_ternary_field_failure();
    test_choice_payload_nested_array_record_constructor_choice_ternary_field_success();
    test_choice_payload_nested_array_record_constructor_pointer_ternary_field_failure();
    test_choice_payload_nested_array_record_constructor_pointer_ternary_field_success();
    test_record_field_choice_payload_array_record_constructor_choice_ternary_field_failure();
    test_record_field_choice_payload_array_record_constructor_choice_ternary_field_success();
    test_record_field_choice_payload_array_record_constructor_pointer_ternary_field_failure();
    test_record_field_choice_payload_array_record_constructor_pointer_ternary_field_success();
    test_record_field_choice_payload_nested_array_record_constructor_choice_ternary_field_failure();
    test_record_field_choice_payload_nested_array_record_constructor_choice_ternary_field_success();
    test_record_field_choice_payload_nested_array_record_constructor_pointer_ternary_field_failure();
    test_record_field_choice_payload_nested_array_record_constructor_pointer_ternary_field_success();
    test_annotated_record_binding_constructor_type_mismatch_failure();
    test_record_return_constructor_type_mismatch_failure();
    test_annotated_integer_binding_same_width_success();
    test_record_assignment_constructor_type_mismatch_failure();
    test_record_field_assignment_constructor_type_mismatch_failure();
    test_function_call_record_argument_type_mismatch_failure();
    test_method_call_record_argument_type_mismatch_failure();
    test_generic_function_dependent_argument_success();
    test_generic_function_dependent_same_width_integer_argument_success();
    test_generic_function_dependent_argument_type_mismatch_failure();
    test_generic_function_repeated_binding_conflict_failure();
    test_generic_method_dependent_same_width_integer_argument_success();
    test_generic_method_repeated_binding_conflict_failure();
    test_generic_method_dependent_argument_type_mismatch_failure();
    test_nested_array_literal_constant_initializer_element_type_failure();
    test_nested_array_literal_constant_initializer_length_failure();
    test_array_literal_constant_initializer_forward_reference_success();
    test_array_literal_constant_initializer_unknown_reference_failure();
    test_array_literal_constant_initializer_direct_cycle_failure();
    test_array_literal_constant_initializer_indirect_cycle_failure();
    test_address_array_literal_constant_initializer_success();
    test_address_array_literal_constant_initializer_element_failure();
    test_pointer_array_literal_constant_initializer_pointer_construction_failure();
    test_choice_array_literal_constant_initializer_success();
    test_choice_array_literal_constant_initializer_payload_type_failure();
    test_nested_choice_array_literal_constant_initializer_success();
    test_nested_choice_array_literal_constant_initializer_payload_type_failure();
    test_array_payload_choice_constant_initializer_success();
    test_array_payload_choice_constant_initializer_length_failure();
    test_array_payload_choice_constant_initializer_element_type_failure();
    test_array_literal_constant_initializer_function_call_failure();
    test_array_literal_constant_initializer_method_call_failure();
    test_array_payload_choice_constant_initializer_function_call_failure();
    test_array_payload_choice_constant_initializer_method_call_failure();
    test_array_literal_constant_initializer_unsafe_intrinsic_failure();
    test_array_payload_choice_constant_initializer_unsafe_intrinsic_failure();
    test_array_literal_constant_initializer_await_failure();
    test_array_payload_choice_constant_initializer_await_failure();
    test_array_literal_constant_initializer_task_failure();
    test_array_payload_choice_constant_initializer_task_failure();
    test_array_literal_constant_initializer_thread_failure();
    test_array_payload_choice_constant_initializer_thread_failure();
    test_address_constant_initializer_structural_failure();
    test_pointer_constant_initializer_structural_failure();
    test_forward_constant_initializer_reference_success();
    test_unknown_constant_initializer_reference_failure();
    test_duplicate_top_level_constant_name_failure();
    test_direct_constant_initializer_cycle_failure();
    test_indirect_constant_initializer_cycle_failure();
    test_constant_initializer_await_failure();
    test_constant_initializer_task_failure();
    test_constant_initializer_thread_failure();
    test_constant_initializer_unsafe_intrinsic_failure();
    test_constant_initializer_unsafe_function_failure();
    test_constant_initializer_function_call_failure();
    test_constant_initializer_method_call_failure();
    test_zero_payload_choice_constant_initializer_success();
    test_payload_choice_constant_initializer_success();
    test_generic_choice_constant_initializer_success();
    test_generic_choice_constant_initializer_payload_type_failure();
    test_generic_choice_constant_repeated_payload_success();
    test_generic_choice_constant_repeated_payload_conflict_failure();
    test_ordinary_choice_constructor_annotated_binding_success();
    test_choice_constructor_unannotated_binding_infers_type_success();
    test_choice_constructor_unannotated_binding_infers_type_failure();
    test_choice_constructor_unannotated_zero_payload_requires_expected_type_failure();
    test_choice_constructor_unannotated_ambiguous_name_requires_expected_type_failure();
    test_ordinary_choice_constructor_return_payload_failure();
    test_ordinary_choice_constructor_final_expression_payload_failure();
    test_final_if_expression_type_mismatch_failure();
    test_final_switch_expression_type_mismatch_failure();
    test_final_if_expression_type_match_success();
    test_final_switch_expression_type_match_success();
    test_final_if_return_expression_type_mismatch_failure();
    test_final_switch_return_expression_type_mismatch_failure();
    test_final_if_return_expression_type_match_success();
    test_final_switch_return_expression_type_match_success();
    test_final_ternary_expression_type_mismatch_failure();
    test_final_ternary_expression_type_match_success();
    test_final_ternary_return_expression_type_mismatch_failure();
    test_final_ternary_return_expression_type_match_success();
    test_final_if_without_else_requires_function_value_failure();
    test_final_unsafe_block_without_value_requires_function_value_failure();
    test_final_switch_non_value_case_requires_function_value_failure();
    test_non_unit_empty_return_requires_value_failure();
    test_unit_empty_return_success();
    test_unit_final_if_without_else_success();
    test_unit_final_unsafe_without_value_success();
    test_unit_final_switch_non_value_case_success();
    test_choice_constructor_final_if_payload_failure();
    test_choice_constructor_final_switch_payload_failure();
    test_choice_constructor_final_if_return_payload_failure();
    test_choice_constructor_final_switch_return_payload_failure();
    test_choice_constructor_final_if_success();
    test_choice_constructor_final_switch_success();
    test_choice_constructor_final_if_return_success();
    test_choice_constructor_final_switch_return_success();
    test_choice_constructor_final_ternary_payload_failure();
    test_choice_constructor_final_ternary_return_payload_failure();
    test_choice_constructor_final_ternary_success();
    test_choice_constructor_final_ternary_return_success();
    test_choice_constructor_nested_final_ternary_payload_types();
    test_choice_constructor_ternary_annotated_binding_payload_types();
    test_choice_constructor_ternary_assignment_payload_types();
    test_choice_constructor_ternary_call_argument_payload_types();
    test_ordinary_choice_constructor_assignment_payload_failure();
    test_ordinary_choice_constructor_call_argument_arity_failure();
    test_zero_payload_choice_constructor_annotated_binding_success();
    test_zero_payload_choice_constructor_return_success();
    test_zero_payload_choice_constructor_final_expression_success();
    test_zero_payload_choice_constructor_assignment_success();
    test_zero_payload_choice_constructor_call_argument_success();
    test_nested_choice_constant_initializer_success();
    test_nested_choice_constant_initializer_payload_type_failure();
    test_nested_zero_payload_choice_constant_initializer_arity_failure();
    test_nested_choice_constant_initializer_wrong_choice_type_failure();
    test_nested_choice_constant_initializer_unknown_constructor_failure();
    test_choice_constant_initializer_wrong_choice_type_failure();
    test_choice_constant_initializer_unknown_constructor_failure();
    test_choice_constant_initializer_arity_failure();
    test_zero_payload_choice_constant_initializer_arity_failure();
    test_choice_constant_initializer_payload_type_failure();
    test_constant_initializer_pointer_construction_failure();
    test_nested_address_of_and_raw_offset_success();
    test_index_access_noninteger_index_failure();
    test_index_access_integer_index_success();
    test_call_unsafe_function_outside_unsafe_context_failure();
    test_call_unsafe_function_inside_unsafe_block_success();
    test_pointer_construction_outside_unsafe_context_failure();
    test_pointer_construction_inside_unsafe_block_success();
    test_pointer_construction_without_argument_failure();
    test_pointer_construction_with_multiple_arguments_failure();
    test_pointer_construction_with_nonaddress_argument_failure();
    test_pointer_construction_with_address_of_argument_success();
    test_pointer_typed_binding_with_mismatched_address_of_source_failure();
    test_pointer_typed_binding_with_matching_address_of_source_success();
    test_pointer_return_with_same_width_address_of_source_success();
    test_pointer_typed_binding_with_nonpointer_initializer_failure();
    test_pointer_typed_binding_with_pointer_initializer_success();
    test_pointer_typed_binding_with_mismatched_raw_offset_source_failure();
    test_pointer_typed_binding_with_matching_raw_offset_source_success();
    test_pointer_typed_binding_ternary_raw_offset_source_types();
    test_pointer_assignment_ternary_raw_offset_source_types();
    test_pointer_call_argument_ternary_raw_offset_source_types();
    test_pointer_method_argument_ternary_raw_offset_source_types();
    test_pointer_return_with_same_width_raw_offset_source_success();
    test_raw_read_typed_binding_result_mismatch_failure();
    test_raw_read_typed_binding_result_match_success();
    test_raw_read_typed_binding_same_width_integer_success();
    test_raw_read_typed_binding_pointer_sized_integer_mismatch_failure();
    test_pointer_typed_binding_with_wrong_typed_name_failure();
    test_pointer_typed_binding_with_mismatched_field_pointer_failure();
    test_pointer_typed_binding_with_same_width_field_pointer_success();
    test_pointer_return_with_nonpointer_expression_failure();
    test_pointer_final_expression_with_nonpointer_expression_failure();
    test_pointer_return_with_pointer_expression_success();
    test_pointer_final_expression_with_pointer_expression_success();
    test_pointer_return_with_mismatched_raw_offset_source_failure();
    test_pointer_return_with_matching_raw_offset_source_success();
    test_pointer_return_ternary_raw_offset_source_types();
    test_pointer_final_ternary_raw_offset_source_types();
    test_pointer_nested_final_container_ternary_raw_offset_source_types();
    test_pointer_return_with_wrong_typed_name_failure();
    test_pointer_return_with_mismatched_helper_pointer_failure();
    test_pointer_return_with_same_width_helper_pointer_success();
    test_pointer_return_with_mismatched_address_of_source_failure();
    test_pointer_return_with_matching_address_of_source_success();
    test_pointer_return_ternary_pointer_construction_source_types();
    test_raw_write_generic_helper_returned_pointer_same_width_success();
    test_raw_write_generic_helper_returned_pointer_mismatch_failure();
    test_raw_write_generic_receiver_method_pointer_same_width_success();
    test_raw_write_generic_receiver_method_pointer_mismatch_failure();
    test_raw_read_return_type_mismatch_failure();
    test_raw_read_final_expression_type_mismatch_failure();
    test_raw_read_rebound_final_expression_type_mismatch_failure();
    test_raw_read_unsafe_block_final_expression_type_mismatch_failure();
    test_raw_read_unsafe_block_rebound_final_expression_type_mismatch_failure();
    test_raw_read_branch_merged_final_expression_type_mismatch_failure();
    test_raw_read_switch_merged_final_expression_type_mismatch_failure();
    test_raw_read_final_if_expression_type_mismatch_failure();
    test_raw_read_final_switch_expression_type_mismatch_failure();
    test_low_level_read_nested_final_container_expression_types("raw_read");
    test_low_level_read_final_container_return_types("raw_read");
    test_low_level_read_final_ternary_expression_types("raw_read");
    test_low_level_read_nested_final_container_ternary_expression_types("raw_read");
    test_raw_read_guard_failure_final_expression_type_success();
    test_raw_read_guard_failure_final_expression_type_mismatch_failure();
    test_raw_read_while_zero_iteration_final_expression_type_success();
    test_raw_read_for_zero_iteration_final_expression_type_success();
    test_raw_read_for_final_expression_type_mismatch_failure();
    test_raw_read_repeat_final_expression_type_mismatch_failure();
    test_raw_read_return_type_match_success();
    test_raw_read_final_expression_type_match_success();
    test_raw_read_rebound_final_expression_type_match_success();
    test_raw_read_unsafe_block_final_expression_type_match_success();
    test_raw_read_unsafe_block_rebound_final_expression_type_match_success();
    test_raw_read_branch_merged_final_expression_type_match_success();
    test_raw_read_switch_merged_final_expression_type_match_success();
    test_raw_read_final_if_expression_type_match_success();
    test_raw_read_final_switch_expression_type_match_success();
    test_raw_read_return_same_width_integer_success();
    test_raw_read_return_pointer_sized_integer_mismatch_failure();
    test_raw_write_value_type_mismatch_failure();
    test_raw_write_constant_value_type_mismatch_failure();
    test_raw_write_value_type_match_success();
    test_raw_write_same_width_integer_value_success();
    test_raw_write_pointer_sized_integer_value_mismatch_failure();
    test_raw_write_computed_integer_sum_success();
    test_raw_write_computed_bitwise_value_success();
    test_raw_write_computed_ternary_pointer_sized_mismatch_failure();
    test_raw_write_rebound_computed_value_success();
    test_raw_write_branch_merged_computed_value_success();
    test_raw_write_branch_merged_pointer_sized_value_mismatch_failure();
    test_raw_write_switch_merged_computed_value_success();
    test_raw_write_switch_merged_pointer_sized_value_mismatch_failure();
    test_raw_write_array_indexed_value_success();
    test_raw_write_bound_array_literal_indexed_value_success();
    test_raw_write_array_indexed_pointer_sized_value_mismatch_failure();
    test_raw_write_member_container_field_indexed_value_success();
    test_raw_write_nested_scalar_field_value_success();
    test_raw_write_nested_scalar_field_computed_value_success();
    test_raw_write_helper_returned_container_indexed_value_success();
    test_raw_write_nested_scalar_field_pointer_sized_mismatch_failure();
    test_raw_write_member_container_field_indexed_pointer_sized_mismatch_failure();
    test_raw_write_integer_literal_value_success();
    test_raw_write_integer_cast_value_success();
    test_raw_write_same_width_integer_cast_success();
    test_raw_write_integer_cast_target_mismatch_failure();
    test_raw_write_pointer_sized_integer_cast_mismatch_failure();
    test_raw_write_recovered_raw_read_value_type_mismatch_failure();
    test_raw_write_recovered_raw_read_integer_cast_success();
    test_raw_write_helper_returned_raw_read_value_type_mismatch_failure();
    test_raw_write_member_returned_raw_read_value_type_mismatch_failure();
    test_raw_write_member_returned_raw_read_integer_cast_success();
    test_raw_write_non_integer_cast_value_mismatch_failure();
    test_raw_write_helper_pointer_constructor_type_mismatch_failure();
    test_raw_write_helper_pointer_constructor_type_match_success();
    test_raw_write_member_helper_pointer_constructor_type_mismatch_failure();
    test_raw_write_member_helper_pointer_constructor_type_match_success();
    test_raw_write_raw_offset_helper_pointer_type_mismatch_failure();
    test_raw_write_raw_offset_helper_pointer_type_match_success();
    test_raw_write_member_raw_offset_helper_pointer_type_mismatch_failure();
    test_raw_write_record_pointer_field_type_mismatch_failure();
    test_member_field_address_inference_enables_pointer_constructor_success();
    test_raw_write_indexed_record_pointer_field_type_mismatch_failure();
    test_indexed_member_field_address_inference_enables_pointer_constructor_success();
    test_rebound_indexed_record_pointer_field_type_mismatch_failure();
    test_rebound_indexed_member_field_address_inference_enables_pointer_constructor_success();
    test_return_rebound_indexed_record_pointer_field_success();
    test_return_rebound_indexed_member_field_address_success();
    test_return_rebound_indexed_pointer_used_by_helper_success();
    test_return_rebound_indexed_address_used_by_pointer_constructor_success();
    test_volatile_read_return_type_mismatch_failure();
    test_volatile_read_final_expression_type_mismatch_failure();
    test_volatile_read_rebound_final_expression_type_mismatch_failure();
    test_volatile_read_unsafe_block_final_expression_type_mismatch_failure();
    test_volatile_read_unsafe_block_rebound_final_expression_type_mismatch_failure();
    test_volatile_read_branch_merged_final_expression_type_mismatch_failure();
    test_volatile_read_switch_merged_final_expression_type_mismatch_failure();
    test_volatile_read_final_if_expression_type_mismatch_failure();
    test_volatile_read_final_switch_expression_type_mismatch_failure();
    test_low_level_read_nested_final_container_expression_types("volatile_read");
    test_low_level_read_final_container_return_types("volatile_read");
    test_low_level_read_final_ternary_expression_types("volatile_read");
    test_low_level_read_nested_final_container_ternary_expression_types("volatile_read");
    test_volatile_read_guard_failure_final_expression_type_success();
    test_volatile_read_guard_failure_final_expression_type_mismatch_failure();
    test_volatile_read_while_zero_iteration_final_expression_type_success();
    test_volatile_read_for_zero_iteration_final_expression_type_success();
    test_volatile_read_for_final_expression_type_mismatch_failure();
    test_volatile_read_repeat_final_expression_type_mismatch_failure();
    test_volatile_read_return_type_match_success();
    test_volatile_read_final_expression_type_match_success();
    test_volatile_read_rebound_final_expression_type_match_success();
    test_volatile_read_unsafe_block_final_expression_type_match_success();
    test_volatile_read_unsafe_block_rebound_final_expression_type_match_success();
    test_volatile_read_branch_merged_final_expression_type_match_success();
    test_volatile_read_switch_merged_final_expression_type_match_success();
    test_volatile_read_final_if_expression_type_match_success();
    test_volatile_read_final_switch_expression_type_match_success();
    test_volatile_read_return_same_width_integer_success();
    test_volatile_read_return_pointer_sized_integer_mismatch_failure();
    test_volatile_read_typed_binding_result_mismatch_failure();
    test_volatile_read_typed_binding_result_match_success();
    test_volatile_read_typed_binding_same_width_integer_success();
    test_volatile_read_typed_binding_pointer_sized_integer_mismatch_failure();
    test_volatile_write_value_type_mismatch_failure();
    test_volatile_write_value_type_match_success();
    test_volatile_write_same_width_integer_value_success();
    test_volatile_write_pointer_sized_integer_value_mismatch_failure();
    test_volatile_write_computed_integer_sum_success();
    test_volatile_write_computed_bitwise_value_success();
    test_volatile_write_computed_ternary_pointer_sized_mismatch_failure();
    test_volatile_write_rebound_computed_value_success();
    test_volatile_write_branch_merged_computed_value_success();
    test_volatile_write_branch_merged_pointer_sized_value_mismatch_failure();
    test_volatile_write_switch_merged_computed_value_success();
    test_volatile_write_switch_merged_pointer_sized_value_mismatch_failure();
    test_volatile_write_array_indexed_value_success();
    test_volatile_write_bound_array_literal_indexed_value_success();
    test_volatile_write_array_indexed_pointer_sized_value_mismatch_failure();
    test_volatile_write_member_container_field_indexed_value_success();
    test_volatile_write_nested_scalar_field_value_success();
    test_volatile_write_nested_scalar_field_computed_value_success();
    test_volatile_write_helper_returned_container_indexed_value_success();
    test_volatile_write_nested_scalar_field_pointer_sized_mismatch_failure();
    test_volatile_write_member_container_field_indexed_pointer_sized_mismatch_failure();
    test_volatile_write_integer_literal_value_success();
    test_volatile_write_integer_cast_value_success();
    test_volatile_write_same_width_integer_cast_success();
    test_volatile_write_integer_cast_target_mismatch_failure();
    test_volatile_write_pointer_sized_integer_cast_mismatch_failure();
    test_volatile_write_recovered_volatile_read_value_type_mismatch_failure();
    test_volatile_write_recovered_volatile_read_integer_cast_success();
    test_volatile_write_helper_returned_volatile_read_value_type_mismatch_failure();
    test_volatile_write_member_returned_volatile_read_value_type_mismatch_failure();
    test_volatile_write_member_returned_volatile_read_integer_cast_success();
    test_volatile_write_non_integer_cast_value_mismatch_failure();
    test_volatile_write_helper_pointer_constructor_type_mismatch_failure();
    test_volatile_write_member_helper_pointer_constructor_type_mismatch_failure();
    test_volatile_write_raw_offset_helper_pointer_type_mismatch_failure();
    test_address_typed_binding_with_nonaddress_initializer_failure();
    test_address_typed_binding_with_address_initializer_success();
    test_address_typed_binding_with_wrong_typed_name_failure();
    test_address_return_with_nonaddress_expression_failure();
    test_address_final_expression_with_nonaddress_expression_failure();
    test_address_return_with_address_expression_success();
    test_address_final_expression_with_address_expression_success();
    test_address_return_ternary_expression_types();
    test_address_nested_final_container_ternary_expression_types();
    test_address_return_with_wrong_typed_name_failure();
    test_address_typed_binding_with_field_address_success();
    test_address_typed_binding_with_indexed_address_success();
    test_address_return_with_helper_returned_address_success();
    test_address_return_with_generic_helper_success();
    test_raw_write_generic_record_pointer_field_same_width_success();
    test_raw_write_generic_record_pointer_field_mismatch_failure();
    test_raw_write_generic_record_scalar_field_same_width_success();
    test_address_return_with_generic_record_field_success();
    test_task_outside_async_function_failure();
    test_thread_outside_async_function_success();
    test_thread_join_receiver_success();
    test_join_non_thread_receiver_failure();
    test_join_async_call_receiver_failure();
    test_thread_value_without_join_failure();
    test_concurrency_capture_classification_success();
    test_thread_capture_owned_parameter_type_failure();
    test_thread_capture_transferable_generic_success();
    test_thread_capture_unconstrained_generic_failure();
    test_thread_capture_transferable_concrete_type_success();
    test_thread_capture_shareable_generic_failure();
    test_task_capture_shareable_generic_success();
    test_task_capture_shareable_concrete_type_success();
    test_thread_capture_shareable_concrete_type_failure();
    test_thread_result_owned_concrete_type_failure();
    test_thread_result_transferable_concrete_type_success();
    test_thread_result_shareable_generic_failure();
    test_task_result_shareable_generic_success();
    test_task_result_shareable_concrete_type_success();
    test_thread_result_shareable_concrete_type_failure();
    test_task_expression_value_boundary_failure();
    test_task_expression_value_return_success();
    test_thread_expression_value_boundary_failure();
    test_thread_expression_value_return_success();
    test_task_capture_mutable_outer_local_failure();
    test_thread_capture_mutable_outer_local_failure();
    test_thread_capture_receiver_this_failure();
    return 0;
}
