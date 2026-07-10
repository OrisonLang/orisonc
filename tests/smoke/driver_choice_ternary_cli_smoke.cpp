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

auto choice_ternary_binding_lines(std::string_view expression) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    let value: Maybe<UInt32> = " + std::string(expression),
            "    return 1",
        }
    );
}

auto choice_ternary_assignment_lines(std::string_view expression) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    var value: Maybe<UInt32> = Empty",
            "    value = " + std::string(expression),
            "    return 1",
        }
    );
}

auto choice_ternary_call_argument_lines(std::string_view expression) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo(flag: Bool) -> UInt32",
            "    return consume(" + std::string(expression) + ")",
        }
    );
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_choice_ternary_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_ternary_binding_payload_type.or",
        choice_ternary_binding_lines("flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_ternary_binding_success.or",
        choice_ternary_binding_lines("flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_ternary_assignment_payload_type.or",
        choice_ternary_assignment_lines("flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_ternary_assignment_success.or",
        choice_ternary_assignment_lines("flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_ternary_call_argument_payload_type.or",
        choice_ternary_call_argument_lines("flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_ternary_call_argument_success.or",
        choice_ternary_call_argument_lines("flag ? Some(1 as UInt32) : Empty")
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
