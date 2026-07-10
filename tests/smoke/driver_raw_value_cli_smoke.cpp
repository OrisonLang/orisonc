#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

auto read_command_output(std::string const& command) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen(command.c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(status == 0);
    return output;
}

auto read_failing_command_output(std::string const& command) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen((command + " 2>&1").c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(status != 0);
    return output;
}

template <typename SourceLines>
void write_lines(std::filesystem::path const& path, SourceLines const& lines) {
    std::ofstream output(path);
    for (auto line : lines) {
        output << line << '\n';
    }
}

template <typename SourceLines>
void assert_cli_parse_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::string_view expected_message
) {
    write_lines(path, lines);

    auto command = executable.string() + " --parse " + path.string();
    auto output = read_failing_command_output(command);
    assert(output.find(expected_message) != std::string::npos);
}

template <typename SourceLines>
void assert_cli_parse_success(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines
) {
    write_lines(path, lines);

    auto command = executable.string() + " --parse " + path.string();
    auto output = read_command_output(command);
    assert(output.find("parsed ") != std::string::npos);
}

auto cli_module_lines(std::vector<std::string> body_lines) -> std::vector<std::string> {
    std::vector<std::string> lines {"package demo.cli"};
    lines.insert(lines.end(), body_lines.begin(), body_lines.end());
    return lines;
}

auto low_level_read_call(std::string_view intrinsic, std::string_view operand) -> std::string {
    return std::string(intrinsic) + "(" + std::string(operand) + ")";
}

auto low_level_final_if_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    if flag",
            "        " + std::string(true_branch),
            "    else",
            "        " + std::string(false_branch),
        }
    );
}

auto low_level_final_switch_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    switch flag",
            "        true => " + std::string(true_branch),
            "        false => " + std::string(false_branch),
        }
    );
}

auto low_level_unsafe_final_if_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    unsafe",
            "        if flag",
            "            " + std::string(true_branch),
            "        else",
            "            " + std::string(false_branch),
        }
    );
}

auto low_level_unsafe_final_switch_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    unsafe",
            "        switch flag",
            "            true => " + std::string(true_branch),
            "            false => " + std::string(false_branch),
        }
    );
}

auto low_level_final_if_unsafe_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    if flag",
            "        unsafe",
            "            " + std::string(true_branch),
            "    else",
            "        " + std::string(false_branch),
        }
    );
}

auto low_level_final_switch_unsafe_lines(
    std::string_view function_line,
    std::string_view true_branch,
    std::string_view false_branch
) -> std::vector<std::string> {
    return cli_module_lines(
        {
            std::string(function_line),
            "    switch flag",
            "        true =>",
            "            unsafe",
            "                " + std::string(true_branch),
            "        false => " + std::string(false_branch),
        }
    );
}

auto low_level_final_read_direct_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    " + low_level_read_call(intrinsic, "p"),
        }
    );
}

auto low_level_final_read_direct_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    " + low_level_read_call(intrinsic, "p"),
        }
    );
}

auto low_level_final_read_rebound_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(p: Pointer<Byte>) -> UInt32",
            "    let value = " + low_level_read_call(intrinsic, "p"),
            "    value",
        }
    );
}

auto low_level_final_read_rebound_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(p: Pointer<Byte>) -> Byte",
            "    let value = " + low_level_read_call(intrinsic, "p"),
            "    value",
        }
    );
}

auto low_level_final_read_unsafe_block_direct_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_word(p: Pointer<Byte>) -> Pointer<Byte>",
            "    unsafe",
            "        " + low_level_read_call(intrinsic, "p"),
        }
    );
}

auto low_level_final_read_unsafe_block_direct_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_byte(p: Pointer<Byte>) -> Byte",
            "    unsafe",
            "        " + low_level_read_call(intrinsic, "p"),
        }
    );
}

auto low_level_final_read_unsafe_block_rebound_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_word(p: Pointer<Byte>) -> UInt32",
            "    unsafe",
            "        let value = " + low_level_read_call(intrinsic, "p"),
            "        value",
        }
    );
}

auto low_level_final_read_unsafe_block_rebound_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_byte(p: Pointer<Byte>) -> Byte",
            "    unsafe",
            "        let value = " + low_level_read_call(intrinsic, "p"),
            "        value",
        }
    );
}

auto low_level_final_read_branch_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    if flag",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    else",
            "        value = " + low_level_read_call(intrinsic, "right"),
            "    value",
        }
    );
}

auto low_level_final_read_branch_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    if flag",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    else",
            "        value = " + low_level_read_call(intrinsic, "right"),
            "    value",
        }
    );
}

auto low_level_final_read_switch_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(selector: UInt32, left: Pointer<Byte>, right: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    switch selector",
            "        0 => value = " + low_level_read_call(intrinsic, "left"),
            "        default => value = " + low_level_read_call(intrinsic, "right"),
            "    value",
        }
    );
}

auto low_level_final_read_switch_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_byte(selector: UInt32, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    switch selector",
            "        0 => value = " + low_level_read_call(intrinsic, "left"),
            "        default => value = " + low_level_read_call(intrinsic, "right"),
            "    value",
        }
    );
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
    return cli_module_lines(
        {
            "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    if flag",
            "        unsafe",
            "            " + low_level_read_call(intrinsic, "left"),
            "    else",
            "        unsafe",
            "            " + low_level_read_call(intrinsic, "right"),
        }
    );
}

auto low_level_final_switch_unsafe_block_read_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return low_level_final_switch_unsafe_lines(
        "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
        low_level_read_call(intrinsic, "left"),
        "1 as UInt32"
    );
}

auto low_level_final_switch_unsafe_block_read_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    switch flag",
            "        true =>",
            "            unsafe",
            "                " + low_level_read_call(intrinsic, "left"),
            "        false =>",
            "            unsafe",
            "                " + low_level_read_call(intrinsic, "right"),
        }
    );
}

auto low_level_final_unsafe_block_ternary_read_mismatch_lines(std::string_view intrinsic)
    -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    unsafe",
            "        flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
        }
    );
}

auto low_level_final_unsafe_block_ternary_read_success_lines(std::string_view intrinsic)
    -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "    unsafe",
            "        flag ? " + low_level_read_call(intrinsic, "left") + " : " +
                low_level_read_call(intrinsic, "right"),
        }
    );
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
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<UInt32>) -> UInt32",
            "    var value = 1 as UInt32",
            "    guard flag else",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_guard_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    guard flag else",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_while_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<UInt32>) -> UInt32",
            "    var value = 1 as UInt32",
            "    while flag",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_for_success_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(items: shared View<Int64>, left: Pointer<UInt32>) -> UInt32",
            "    var value = 1 as UInt32",
            "    for item in items",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_for_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(items: shared View<Int64>, left: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    for item in items",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    value",
        }
    );
}

auto low_level_final_read_repeat_mismatch_lines(std::string_view intrinsic) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "    var value = " + low_level_read_call(intrinsic, "left"),
            "    repeat",
            "        value = " + low_level_read_call(intrinsic, "left"),
            "    while flag",
            "    value",
        }
    );
}

void assert_cli_nested_final_container_parse_cases(
    std::filesystem::path const& executable,
    std::filesystem::path const& root,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_unsafe_block_final_if_expression_type.or"),
        low_level_final_unsafe_block_if_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_unsafe_block_final_if_expression_success.or"),
        low_level_final_unsafe_block_if_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_unsafe_block_final_switch_expression_type.or"),
        low_level_final_unsafe_block_switch_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_unsafe_block_final_switch_expression_success.or"),
        low_level_final_unsafe_block_switch_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_final_if_unsafe_block_expression_type.or"),
        low_level_final_if_unsafe_block_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_final_if_unsafe_block_expression_success.or"),
        low_level_final_if_unsafe_block_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_final_switch_unsafe_block_expression_type.or"),
        low_level_final_switch_unsafe_block_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_final_switch_unsafe_block_expression_success.or"),
        low_level_final_switch_unsafe_block_read_success_lines(intrinsic)
    );
}

void assert_cli_final_container_return_parse_cases(
    std::filesystem::path const& executable,
    std::filesystem::path const& root,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_final_if_return_type.or"),
        low_level_final_if_lines(
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "return " + low_level_read_call(intrinsic, "left"),
            "return 1 as UInt32"
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_final_if_return_success.or"),
        low_level_final_if_lines(
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "return " + low_level_read_call(intrinsic, "left"),
            "return " + low_level_read_call(intrinsic, "right")
        )
    );
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_final_switch_return_type.or"),
        low_level_final_switch_lines(
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "return " + low_level_read_call(intrinsic, "left"),
            "return 1 as UInt32"
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_final_switch_return_success.or"),
        low_level_final_switch_lines(
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "return " + low_level_read_call(intrinsic, "left"),
            "return " + low_level_read_call(intrinsic, "right")
        )
    );
}

void assert_cli_final_ternary_low_level_parse_cases(
    std::filesystem::path const& executable,
    std::filesystem::path const& root,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_final_ternary_expression_type.or"),
        cli_module_lines(
            {
                "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
                "    flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
            }
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_final_ternary_expression_success.or"),
        cli_module_lines(
            {
                "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
                "    flag ? " + low_level_read_call(intrinsic, "left") + " : " +
                    low_level_read_call(intrinsic, "right"),
            }
        )
    );
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_final_ternary_return_type.or"),
        cli_module_lines(
            {
                "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
                "    return flag ? " + low_level_read_call(intrinsic, "left") + " : 1 as UInt32",
            }
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_final_ternary_return_success.or"),
        cli_module_lines(
            {
                "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
                "    return flag ? " + low_level_read_call(intrinsic, "left") + " : " +
                    low_level_read_call(intrinsic, "right"),
            }
        )
    );
}

void assert_cli_nested_final_ternary_low_level_parse_cases(
    std::filesystem::path const& executable,
    std::filesystem::path const& root,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_unsafe_block_final_ternary_type.or"),
        low_level_final_unsafe_block_ternary_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_unsafe_block_final_ternary_success.or"),
        low_level_final_unsafe_block_ternary_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_final_if_ternary_type.or"),
        low_level_final_if_ternary_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_final_if_ternary_success.or"),
        low_level_final_if_ternary_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / (prefix + "_final_switch_ternary_type.or"),
        low_level_final_switch_ternary_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / (prefix + "_final_switch_ternary_success.or"),
        low_level_final_switch_ternary_read_success_lines(intrinsic)
    );
}

void assert_cli_raw_read_parse_cases(
    std::filesystem::path const& executable,
    std::filesystem::path const& root,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'Pointer<Byte>'";
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_final_expression_type.or"),
        low_level_final_read_direct_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_final_expression_success.or"),
        low_level_final_read_direct_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_rebound_final_expression_type.or"),
        low_level_final_read_rebound_mismatch_lines(intrinsic),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_rebound_final_expression_success.or"),
        low_level_final_read_rebound_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_unsafe_block_final_expression_type.or"),
        low_level_final_read_unsafe_block_direct_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_unsafe_block_final_expression_success.or"),
        low_level_final_read_unsafe_block_direct_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_unsafe_block_rebound_final_expression_type.or"),
        low_level_final_read_unsafe_block_rebound_mismatch_lines(intrinsic),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_unsafe_block_rebound_final_expression_success.or"),
        low_level_final_read_unsafe_block_rebound_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_branch_final_expression_type.or"),
        low_level_final_read_branch_mismatch_lines(intrinsic),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_branch_final_expression_success.or"),
        low_level_final_read_branch_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_switch_final_expression_type.or"),
        low_level_final_read_switch_mismatch_lines(intrinsic),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_switch_final_expression_success.or"),
        low_level_final_read_switch_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_final_if_expression_type.or"),
        low_level_final_if_read_mismatch_lines(intrinsic),
        std::string(intrinsic) + " result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_final_if_expression_success.or"),
        low_level_final_if_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_final_switch_expression_type.or"),
        low_level_final_switch_read_mismatch_lines(intrinsic),
        std::string(intrinsic) + " result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_final_switch_expression_success.or"),
        low_level_final_switch_read_success_lines(intrinsic)
    );
    assert_cli_nested_final_container_parse_cases(executable, root, intrinsic);
    assert_cli_final_container_return_parse_cases(executable, root, intrinsic);
    assert_cli_final_ternary_low_level_parse_cases(executable, root, intrinsic);
    assert_cli_nested_final_ternary_low_level_parse_cases(executable, root, intrinsic);
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_guard_final_expression_success.or"),
        low_level_final_read_guard_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_guard_final_expression_type.or"),
        low_level_final_read_guard_mismatch_lines(intrinsic),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_while_final_expression_success.or"),
        low_level_final_read_while_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_repeat_final_expression_type.or"),
        low_level_final_read_repeat_mismatch_lines(intrinsic),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_for_final_expression_success.or"),
        low_level_final_read_for_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        root / ("orison_cli_" + std::string(intrinsic) + "_for_final_expression_type.or"),
        low_level_final_read_for_mismatch_lines(intrinsic),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_raw_value_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_raw_read_parse_cases(executable, smoke_temp_root, "raw_read");
    assert_cli_raw_read_parse_cases(executable, smoke_temp_root, "volatile_read");

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
