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

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_address_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_address_final_expression_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function base() -> Address",
            "    \"text\"",
        },
        "address-returning function currently requires a structurally address-like expression"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_address_final_expression_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function base(buf: exclusive Buffer) -> Address",
            "    address_of(buf.data[0])",
        }
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_address_return_ternary_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    return flag ? address_of(buf.data[0]) : \"text\"",
        },
        "address-returning function currently requires a structurally address-like expression"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_address_unsafe_final_ternary_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    unsafe",
            "        flag ? address_of(buf.data[0]) : address_of(buf.data[1])",
        }
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
