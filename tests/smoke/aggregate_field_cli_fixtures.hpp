#pragma once

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
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
void assert_cli_parse_failure(
    std::filesystem::path const& executable,
    std::filesystem::path const& path,
    SourceLines const& lines,
    std::string_view expected_message
) {
    {
        std::ofstream output(path);
        for (auto line : lines) {
            output << line << '\n';
        }
    }

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
    {
        std::ofstream output(path);
        for (auto line : lines) {
            output << line << '\n';
        }
    }

    auto command = executable.string() + " --parse " + path.string();
    auto output = read_command_output(command);
    assert(output.find("parsed ") != std::string::npos);
}

auto cli_module_lines(std::vector<std::string> body_lines) -> std::vector<std::string> {
    std::vector<std::string> lines {"package demo.cli"};
    lines.insert(lines.end(), body_lines.begin(), body_lines.end());
    return lines;
}

auto box_maybe_items_cli_lines(
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
    return cli_module_lines(lines);
}

auto slot_pointer_items_cli_lines(
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
    return cli_module_lines(lines);
}

auto slot_pointer_items_success_cli_lines(
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
)
    -> std::vector<std::string> {
    auto lines = slot_pointer_items_cli_lines(binding_line, include_holder, payload_type);
    auto const function_index = include_holder ? std::size_t {7} : std::size_t {5};
    lines[function_index] =
        "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_items_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    var item: Wrap<UInt32> = Items([[Box(Some(1 as UInt32))]])",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_items_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    var item: Wrap<UInt32> = Items([[Slot(raw_offset(other, 1))]])",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_items_assignment_success_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    auto lines = slot_pointer_items_assignment_cli_lines(assignment_line);
    lines[5] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_holder_items_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "function demo(flag: Bool) -> UInt32",
            "    var holder: Holder<UInt32> = Holder(Items([[Box(Some(1 as UInt32))]]))",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_holder_items_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    var holder: Holder<UInt32> = Holder(Items([[Slot(raw_offset(other, 1))]]))",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_holder_items_assignment_success_cli_lines(std::string_view assignment_line)
    -> std::vector<std::string> {
    auto lines = slot_pointer_holder_items_assignment_cli_lines(assignment_line);
    lines[7] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_items_return_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> Wrap<UInt32>",
            std::string(return_line),
        }
    );
}

auto slot_pointer_items_return_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Wrap<UInt32>",
            std::string(return_line),
        }
    );
}

auto slot_pointer_items_return_success_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    auto lines = slot_pointer_items_return_cli_lines(return_line);
    lines[5] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> Wrap<UInt32>";
    return lines;
}

auto box_maybe_items_final_if_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> Wrap<UInt32>",
            "    if flag",
            std::string(true_branch),
            "    else",
            "        Items([[Box(Some(1 as UInt32))]])",
        }
    );
}

auto slot_pointer_items_final_if_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Wrap<UInt32>",
            "    if flag",
            std::string(true_branch),
            "    else",
            "        Items([[Slot(raw_offset(other, 1))]])",
        }
    );
}

auto slot_pointer_items_final_if_success_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    auto lines = slot_pointer_items_final_if_cli_lines(true_branch);
    lines[5] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> Wrap<UInt32>";
    return lines;
}

auto box_maybe_holder_items_return_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "function demo(flag: Bool) -> Holder<UInt32>",
            std::string(return_line),
        }
    );
}

auto slot_pointer_holder_items_return_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Holder<UInt32>",
            std::string(return_line),
        }
    );
}

auto slot_pointer_holder_items_return_success_cli_lines(std::string_view return_line) -> std::vector<std::string> {
    auto lines = slot_pointer_holder_items_return_cli_lines(return_line);
    lines[7] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> Holder<UInt32>";
    return lines;
}

auto box_maybe_holder_items_final_if_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Box<T>, 1>, 1>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "function demo(flag: Bool) -> Holder<UInt32>",
            "    if flag",
            std::string(true_branch),
            "    else",
            "        Holder(Items([[Box(Some(1 as UInt32))]]))",
        }
    );
}

auto slot_pointer_holder_items_final_if_cli_lines(std::string_view true_branch) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Items(value: Array<Array<Slot<T>, 1>, 1>)",
            "record Holder<T>",
            "    wrap: Wrap<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Holder<UInt32>",
            "    if flag",
            std::string(true_branch),
            "    else",
            "        Holder(Items([[Slot(raw_offset(other, 1))]]))",
        }
    );
}

auto slot_pointer_holder_items_final_if_success_cli_lines(std::string_view true_branch)
    -> std::vector<std::string> {
    auto lines = slot_pointer_holder_items_final_if_cli_lines(true_branch);
    lines[7] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> Holder<UInt32>";
    return lines;
}

void assert_cli_choice_payload_items_choice_ternary_failure(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Box<T>, 1>"
) {
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        box_maybe_items_cli_lines(binding_line, include_holder, payload_type),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
}

void assert_cli_choice_payload_items_choice_ternary_success(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Box<T>, 1>"
) {
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        box_maybe_items_cli_lines(binding_line, include_holder, payload_type)
    );
}

void assert_cli_choice_payload_items_pointer_ternary_failure(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
) {
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        slot_pointer_items_cli_lines(binding_line, include_holder, payload_type),
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
}

void assert_cli_choice_payload_items_pointer_ternary_success(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    std::string_view binding_line,
    bool include_holder,
    std::string_view payload_type = "Array<Slot<T>, 1>"
) {
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        slot_pointer_items_success_cli_lines(binding_line, include_holder, payload_type)
    );
}

auto box_maybe_record_cli_lines(std::string_view binding_line, bool include_outer) -> std::vector<std::string> {
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
    return cli_module_lines(lines);
}

auto slot_pointer_record_cli_lines(std::string_view binding_line, bool include_wrapper) -> std::vector<std::string> {
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
    return cli_module_lines(lines);
}

auto slot_pointer_record_success_cli_lines(std::string_view binding_line, bool include_wrapper)
    -> std::vector<std::string> {
    auto lines = slot_pointer_record_cli_lines(binding_line, include_wrapper);
    auto const function_index = include_wrapper ? std::size_t {5} : std::size_t {3};
    lines[function_index] =
        "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto box_maybe_nested_array_field_cli_lines(std::string_view binding_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
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
        }
    );
}

auto slot_pointer_nested_array_field_cli_lines(std::string_view binding_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "record Rack<T>",
            "    slots: Array<Array<Slot<T>, 1>, 1>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            std::string(binding_line),
            "    return 1",
        }
    );
}

auto slot_pointer_nested_array_field_success_cli_lines(std::string_view binding_line) -> std::vector<std::string> {
    auto lines = slot_pointer_nested_array_field_cli_lines(binding_line);
    lines[5] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

auto maybe_nested_array_field_cli_lines(std::string_view binding_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record ChoiceShelf<T>",
            "    values: Array<Array<Maybe<T>, 1>, 1>",
            "function demo(flag: Bool) -> UInt32",
            std::string(binding_line),
            "    return 1",
        }
    );
}

auto box_maybe_nested_array_argument_cli_lines(
    std::string_view call_line,
    bool method_call = false
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "record Box<T>",
        "    value: Maybe<T>",
    };
    if (method_call) {
        lines.push_back("record Consumer");
        lines.push_back("    id: UInt32");
        lines.push_back("extend Consumer");
        lines.push_back("    function consume(this: shared This, values: Array<Array<Box<UInt32>, 1>, 1>) -> UInt32");
        lines.push_back("        return 1");
        lines.push_back("function demo(consumer: Consumer, flag: Bool) -> UInt32");
    } else {
        lines.push_back("function consume(values: Array<Array<Box<UInt32>, 1>, 1>) -> UInt32");
        lines.push_back("    return 1");
        lines.push_back("function demo(flag: Bool) -> UInt32");
    }
    lines.push_back(std::string(call_line));
    return cli_module_lines(lines);
}

auto maybe_nested_array_argument_cli_lines(
    std::string_view call_line,
    bool method_call = false
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
    };
    if (method_call) {
        lines.push_back("record Consumer");
        lines.push_back("    id: UInt32");
        lines.push_back("extend Consumer");
        lines.push_back("    function consume(this: shared This, values: Array<Array<Maybe<UInt32>, 1>, 1>) -> UInt32");
        lines.push_back("        return 1");
        lines.push_back("function demo(consumer: Consumer, flag: Bool) -> UInt32");
    } else {
        lines.push_back("function consume(values: Array<Array<Maybe<UInt32>, 1>, 1>) -> UInt32");
        lines.push_back("    return 1");
        lines.push_back("function demo(flag: Bool) -> UInt32");
    }
    lines.push_back(std::string(call_line));
    return cli_module_lines(lines);
}

auto box_maybe_nested_array_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    var values: Array<Array<Box<UInt32>, 1>, 1> = [[Box(Some(1 as UInt32))]]",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto maybe_nested_array_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    var values: Array<Array<Maybe<UInt32>, 1>, 1> = [[Some(1 as UInt32)]]",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_nested_array_assignment_cli_lines(std::string_view assignment_line) -> std::vector<std::string> {
    return cli_module_lines(
        {
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    var slots: Array<Array<Slot<UInt32>, 1>, 1> = [[Slot(raw_offset(other, 1))]]",
            std::string(assignment_line),
            "    return 1",
        }
    );
}

auto slot_pointer_nested_array_assignment_success_cli_lines(std::string_view assignment_line)
    -> std::vector<std::string> {
    auto lines = slot_pointer_nested_array_assignment_cli_lines(assignment_line);
    lines[3] = "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32";
    return lines;
}

template <typename SourceLines>
void assert_cli_record_field_nested_array_choice_context_failure(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    SourceLines const& lines
) {
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        lines,
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
}

template <typename SourceLines>
void assert_cli_record_field_nested_array_pointer_context_failure(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    SourceLines const& lines
) {
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        lines,
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
}

template <typename SourceLines>
void assert_cli_record_field_nested_array_context_success(
    std::filesystem::path const& executable,
    std::string_view fixture_name,
    SourceLines const& lines
) {
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / std::string(fixture_name),
        lines
    );
}

} // namespace
