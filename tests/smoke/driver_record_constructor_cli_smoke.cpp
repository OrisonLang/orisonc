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

auto header_record_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        initializer,
    };
}

auto header_record_function_lines(std::initializer_list<std::string_view> body_lines) -> std::vector<std::string_view> {
    std::vector<std::string_view> lines {
        "package demo.cli",
        "record Header",
        "    magic: Array<UInt32, 2>",
        "    version: UInt16",
        "function default_header() -> Header",
    };
    lines.insert(lines.end(), body_lines.begin(), body_lines.end());
    return lines;
}

auto two_header_record_function_lines(
    std::string_view return_type,
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
    };
    lines.push_back("function default_header() -> " + std::string(return_type));
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto two_header_record_consumer_lines(
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
        "function consume_header(header: Header) -> UInt16",
        "    return header.version",
        "function demo() -> UInt16",
    };
    for (auto line : body_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

}  // namespace

auto main() -> int {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root = original_temp_root /
        ("orison_driver_record_constructor_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_record_constructor_constant_field_type.or",
        header_record_constant_lines("const DEFAULT_HEADER: Header = Header([1, 2], true)"),
        "record constructor field 'version' type 'Bool' does not match expected field type 'UInt16'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_record_constructor_return_arity.or",
        header_record_function_lines({"    return Header([1, 2])"}),
        "record constructor 'Header' expects 2 field values but received 1"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_record_constructor_return_type.or",
        two_header_record_function_lines("OtherHeader", {"    return Header([1, 2], 1)"}),
        "return expression type 'Header' does not match declared type 'OtherHeader'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_record_constructor_assignment_type.or",
        two_header_record_function_lines(
            "Header",
            {
                "    var header = Header([1, 2], 1)",
                "    header = OtherHeader([1, 2], 1)",
                "    return header",
            }
        ),
        "assignment value type 'OtherHeader' does not match declared type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_record_constructor_function_argument_type.or",
        two_header_record_consumer_lines({"    return consume_header(OtherHeader([1, 2], 1))"}),
        "function argument 'header' type 'OtherHeader' does not match declared type 'Header'"
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
