#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
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

auto generic_pair_consumer_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
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
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto generic_same_consumer_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "record OtherHeader",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "function same<T>(left: T, right: T) -> UInt16",
        "    return 1",
        "function demo() -> UInt16",
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto generic_same_record_lines(
    std::initializer_list<std::string_view> body_lines
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
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
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto generic_same_record_for_lines() -> std::vector<std::string> {
    return {
        "package demo.cli",
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
        "    var total: UInt16 = 0 as UInt16",
        "    for item in [Same(Header([1, 2], 1), OtherHeader([1, 2], 1))]",
        "        total = total + item.first.version",
        "    total",
    };
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orison_driver_generic_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_function_dependent_argument_type.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(OtherHeader([1, 2], 1), 1 as UInt16))"}
        ),
        "function argument 'pair' type 'Pair<OtherHeader, UInt16>' does not match declared type 'Pair<Header, UInt16>'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_repeated_binding_argument_type.or",
        generic_same_consumer_lines({"    return same(Header([1, 2], 1), OtherHeader([1, 2], 1))"}),
        "function argument 'right' type 'OtherHeader' does not match declared type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_record_repeated_field_type.or",
        generic_same_record_lines(
            {
                "    let same = Same(Header([1, 2], 1), OtherHeader([1, 2], 1))",
                "    return same.first.version",
            }
        ),
        "record constructor field 'second' type 'OtherHeader' does not match expected field type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_generic_record_array_literal_for_repeated_field_type.or",
        generic_same_record_for_lines(),
        "record constructor field 'second' type 'OtherHeader' does not match expected field type 'Header'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_generic_function_dependent_same_width_integer.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(Header([1, 2], 1), 1 as Int16))"}
        )
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
