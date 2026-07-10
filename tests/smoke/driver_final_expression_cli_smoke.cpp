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

auto ordinary_final_if_lines(std::string_view true_branch, std::string_view false_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function demo(flag: Bool) -> UInt32",
            "    if flag",
            "        " + std::string(true_branch),
            "    else",
            "        " + std::string(false_branch),
        }
    );
}

auto ordinary_final_switch_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function demo(flag: Bool) -> UInt32",
            "    switch flag",
            "        true => " + std::string(true_branch),
            "        false => " + std::string(false_branch),
        }
    );
}

auto ordinary_final_ternary_lines(std::string_view expression) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "function demo(flag: Bool) -> UInt32",
            "    " + std::string(expression),
        }
    );
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_final_expression_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_final_if_expression_type.or",
        ordinary_final_if_lines("true", "1 as UInt32"),
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_final_switch_expression_type.or",
        ordinary_final_switch_lines("true", "1 as UInt32"),
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_final_if_expression_success.or",
        ordinary_final_if_lines("1 as UInt32", "2 as UInt32")
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_final_switch_expression_success.or",
        ordinary_final_switch_lines("1 as UInt32", "2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_final_if_return_type.or",
        ordinary_final_if_lines("return true", "return 1 as UInt32"),
        "return expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_final_switch_return_type.or",
        ordinary_final_switch_lines("return true", "return 1 as UInt32"),
        "return expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_final_if_return_success.or",
        ordinary_final_if_lines("return 1 as UInt32", "return 2 as UInt32")
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_final_switch_return_success.or",
        ordinary_final_switch_lines("return 1 as UInt32", "return 2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_final_ternary_expression_type.or",
        ordinary_final_ternary_lines("flag ? true : 1 as UInt32"),
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_final_ternary_expression_success.or",
        ordinary_final_ternary_lines("flag ? 1 as UInt32 : 2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_final_ternary_return_type.or",
        ordinary_final_ternary_lines("return flag ? true : 1 as UInt32"),
        "return expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_final_ternary_return_success.or",
        ordinary_final_ternary_lines("return flag ? 1 as UInt32 : 2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_final_if_without_else_value.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> UInt32",
            "    if flag",
            "        1 as UInt32",
        },
        "function body must end with an expression statement, value return, or total final-expression container"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_final_unsafe_without_value.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> UInt32",
            "    unsafe",
            "        let value = 1 as UInt32",
        },
        "function body must end with an expression statement, value return, or total final-expression container"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_final_switch_non_value_case.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> UInt32",
            "    switch flag",
            "        true =>",
            "            let value = 1 as UInt32",
            "        false => 2 as UInt32",
        },
        "function body must end with an expression statement, value return, or total final-expression container"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_non_unit_empty_return.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> UInt32",
            "    return",
        },
        "return statement must return a value for declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_unit_empty_return.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> Unit",
            "    return",
        }
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_unit_final_if_without_else.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> Unit",
            "    if flag",
            "        let value = 1 as UInt32",
        }
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_unit_final_unsafe_without_value.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> Unit",
            "    unsafe",
            "        let value = 1 as UInt32",
        }
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_unit_final_switch_non_value_case.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> Unit",
            "    switch flag",
            "        true =>",
            "            let value = 1 as UInt32",
            "        false =>",
            "            let value = 2 as UInt32",
        }
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
