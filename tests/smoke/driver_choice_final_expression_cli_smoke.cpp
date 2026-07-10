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

auto choice_final_if_lines(std::string_view true_branch, std::string_view false_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    if flag",
            "        " + std::string(true_branch),
            "    else",
            "        " + std::string(false_branch),
        }
    );
}

auto choice_final_switch_lines(std::string_view true_branch, std::string_view false_branch)
    -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    switch flag",
            "        true => " + std::string(true_branch),
            "        false => " + std::string(false_branch),
        }
    );
}

auto choice_final_ternary_lines(std::string_view expression) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    " + std::string(expression),
        }
    );
}

auto choice_final_unsafe_block_ternary_lines(std::string_view expression) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> Maybe<UInt32>",
            "    unsafe",
            "        " + std::string(expression),
        }
    );
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

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_choice_final_expression_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_final_if_payload_type.or",
        choice_final_if_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_final_switch_payload_type.or",
        choice_final_switch_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_final_if_return_payload_type.or",
        choice_final_if_lines("return Some(true)", "return Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_final_switch_return_payload_type.or",
        choice_final_switch_lines("return Some(true)", "return Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_final_if_success.or",
        choice_final_if_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_final_switch_success.or",
        choice_final_switch_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_final_if_return_success.or",
        choice_final_if_lines("return Some(1 as UInt32)", "return Empty")
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_final_switch_return_success.or",
        choice_final_switch_lines("return Some(1 as UInt32)", "return Empty")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_final_ternary_payload_type.or",
        choice_final_ternary_lines("flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_final_ternary_return_payload_type.or",
        choice_final_ternary_lines("return flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_final_ternary_success.or",
        choice_final_ternary_lines("flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_final_ternary_return_success.or",
        choice_final_ternary_lines("return flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_unsafe_final_ternary_payload_type.or",
        choice_final_unsafe_block_ternary_lines("flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_unsafe_final_ternary_success.or",
        choice_final_unsafe_block_ternary_lines("flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_final_if_ternary_payload_type.or",
        choice_final_if_ternary_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_final_if_ternary_success.or",
        choice_final_if_ternary_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_final_switch_ternary_payload_type.or",
        choice_final_switch_ternary_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_final_switch_ternary_success.or",
        choice_final_switch_ternary_lines("Some(1 as UInt32)", "Empty")
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
