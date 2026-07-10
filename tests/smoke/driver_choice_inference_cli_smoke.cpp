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

auto maybe_choice_lines(std::vector<std::string> body_lines) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
    };
    lines.insert(lines.end(), body_lines.begin(), body_lines.end());
    return cli_module_lines(lines);
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_choice_inference_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_zero_payload_final_expression.or",
        maybe_choice_lines(
            {
                "function demo() -> Maybe<UInt32>",
                "    Empty",
            }
        )
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_unannotated_binding_type.or",
        maybe_choice_lines(
            {
                "function consume(value: Maybe<UInt32>) -> UInt32",
                "    return 1",
                "function demo() -> UInt32",
                "    let value = Some(1 as UInt32)",
                "    return consume(value)",
            }
        )
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_unannotated_zero_payload.or",
        maybe_choice_lines(
            {
                "function demo() -> UInt32",
                "    let value = Empty",
                "    return 1",
            }
        ),
        "choice constructor 'Empty' requires an expected choice type"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_choice_unannotated_ambiguous_name.or",
        cli_module_lines(
            {
                "choice LocalStatus",
                "    Ready(value: UInt32)",
                "choice RemoteStatus",
                "    Ready(value: UInt32)",
                "function demo() -> UInt32",
                "    let value = Ready(1 as UInt32)",
                "    return 1",
            }
        ),
        "choice constructor 'Ready' requires an expected choice type"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_choice_zero_payload_call_argument.or",
        maybe_choice_lines(
            {
                "function consume(value: Maybe<UInt32>) -> UInt32",
                "    return 1",
                "function demo() -> UInt32",
                "    return consume(Empty)",
            }
        )
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
