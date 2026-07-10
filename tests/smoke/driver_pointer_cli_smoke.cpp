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
        original_temp_root / ("orison_driver_pointer_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";

    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_pointer_final_expression_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    \"text\"",
        },
        "pointer-returning function currently requires a structurally pointer-like expression"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_pointer_final_expression_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    raw_offset(base, 1)",
        }
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_pointer_return_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_pointer_binding_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    let p: Pointer<UInt32> = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_pointer_binding_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    let p: Pointer<Byte> = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        }
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_pointer_assignment_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    var p: Pointer<UInt32> = raw_offset(other, 1)",
            "    p = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_pointer_assignment_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    var p: Pointer<Byte> = raw_offset(other, 1)",
            "    p = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        }
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_pointer_call_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function consume(ptr: Pointer<UInt32>) -> UInt32",
            "    return 1",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    return consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_pointer_call_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function consume(ptr: Pointer<Byte>) -> UInt32",
            "    return 1",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> UInt32",
            "    return consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        }
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_pointer_call_address_offset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Registers",
            "    status: UInt32",
            "unsafe function consume(ptr: Pointer<UInt32>) -> UInt32",
            "    return raw_read(ptr)",
            "unsafe function demo() -> UInt32",
            "    var regs = Registers(1 as UInt32)",
            "    return consume(raw_offset(Pointer(address_of(regs.status)), 1 as UInt64))",
        }
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_pointer_method_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Device",
            "    status: UInt32",
            "extend Device",
            "    function consume(this: shared This, ptr: Pointer<UInt32>) -> UInt32",
            "        return 1",
            "unsafe function demo(device: Device, flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    return device.consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_pointer_method_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Device",
            "    status: UInt32",
            "extend Device",
            "    function consume(this: shared This, ptr: Pointer<Byte>) -> UInt32",
            "        return 1",
            "unsafe function demo(device: Device, flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> UInt32",
            "    return device.consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        }
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_pointer_method_address_offset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Registers",
            "    status: UInt32",
            "record Device",
            "    status: UInt32",
            "extend Device",
            "    function consume(this: shared This, ptr: Pointer<UInt32>) -> UInt32",
            "        return 1 as UInt32",
            "unsafe function demo(device: Device) -> UInt32",
            "    var regs = Registers(1 as UInt32)",
            "    return device.consume(raw_offset(Pointer(address_of(regs.status)), 1 as UInt64))",
        }
    );
    assert_cli_parse_success(
        executable,
        smoke_temp_root / "orison_cli_pointer_final_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        }
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_pointer_final_if_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    if flag",
            "        flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    else",
            "        raw_offset(other, 1)",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        smoke_temp_root / "orison_cli_pointer_ternary_pointer_source_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_word_ptr(flag: Bool, buf: Buffer, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return flag ? Pointer(address_of(buf.data[0])) : raw_offset(other, 1)",
        },
        "Pointer construction source type 'Byte' does not match expected pointer element type 'UInt32'"
    );

    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
