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
#include <utility>
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

auto read_command_output_with_exit_code(std::string const& command, int expected_exit_code) -> std::string {
    std::array<char, 256> buffer {};
    std::string output;

    FILE* pipe = popen(command.c_str(), "r");
    assert(pipe != nullptr);

    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    auto status = pclose(pipe);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == expected_exit_code);
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
void assert_cli_emit_llvm_failure(
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

    auto command = executable.string() + " --emit-llvm " + path.string();
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

auto status_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        "choice Status",
        "    Ready(code: UInt32)",
        "    Empty",
        initializer,
    };
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

auto maybe_choice_constant_lines_with_declarations(
    std::string_view initializer,
    std::initializer_list<std::string_view> declarations,
    std::initializer_list<std::string_view> trailing_lines = {}
) -> std::vector<std::string_view> {
    std::vector<std::string_view> lines {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
    };
    lines.insert(lines.end(), declarations.begin(), declarations.end());
    lines.push_back(initializer);
    lines.insert(lines.end(), trailing_lines.begin(), trailing_lines.end());
    return lines;
}

auto maybe_choice_constant_lines(
    std::string_view initializer,
    std::initializer_list<std::string_view> trailing_lines = {}
) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(initializer, {}, trailing_lines);
}

auto boxed_maybe_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(
        initializer,
        {"choice Boxed<T>", "    Wrap(inner: T)"}
    );
}

auto maybe_result_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(
        initializer,
        {"choice Result<T>", "    Ok(value: T)", "    Error(message: Text)"}
    );
}

auto boxed_maybe_result_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return maybe_choice_constant_lines_with_declarations(
        initializer,
        {
            "choice Boxed<T>",
            "    Wrap(inner: T)",
            "choice Result<T>",
            "    Ok(value: T)",
            "    Error(message: Text)",
        }
    );
}

auto boxed_maybe_array_choice_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return boxed_maybe_choice_constant_lines(initializer);
}

auto scalar_array_constant_lines(
    std::string initializer,
    std::initializer_list<std::string_view> trailing_lines = {}
) -> std::vector<std::string> {
    std::vector<std::string> lines {
        "package demo.cli",
        std::move(initializer),
    };
    for (auto line : trailing_lines) {
        lines.emplace_back(line);
    }
    return lines;
}

auto scalar_array_constant_initializer(std::string_view elements, std::size_t length = 1) -> std::string {
    return "const MAGIC: Array<UInt32, " + std::to_string(length) + "> = [" + std::string(elements) + "]";
}

auto nested_array_constant_initializer(std::string_view elements, std::size_t length = 1) -> std::string {
    return "const MATRIX: Array<Array<UInt32, 2>, " + std::to_string(length) + "> = [" + std::string(elements) + "]";
}

auto scalar_array_block_runtime_constant_lines(std::string_view construct) -> std::vector<std::string_view> {
    if (construct == "task") {
        return {
            "package demo.cli",
            "const MAGIC: Array<UInt32, 1> = [task",
            "    1",
            "]",
        };
    }

    return {
        "package demo.cli",
        "const MAGIC: Array<UInt32, 1> = [thread",
        "    1",
        "]",
    };
}

auto maybe_array_payload_block_runtime_constant_lines(std::string_view construct) -> std::vector<std::string_view> {
    if (construct == "task") {
        return {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([task",
            "    1",
            "])",
        };
    }

    return {
        "package demo.cli",
        "choice Maybe<T>",
        "    Some(value: T)",
        "    Empty",
        "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([thread",
        "    1",
        "])",
    };
}

auto nested_array_constant_lines(std::string initializer) -> std::vector<std::string> {
    return {
        "package demo.cli",
        std::move(initializer),
    };
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

auto low_level_array_constant_lines(std::string_view initializer) -> std::vector<std::string_view> {
    return {
        "package demo.cli",
        initializer,
        "const UART0_DATA: Address = 0x4000_1000",
    };
}

void assert_cli_nested_final_container_parse_cases(
    std::filesystem::path const& executable,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_if_expression_type.or"),
        low_level_final_unsafe_block_if_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_if_expression_success.or"),
        low_level_final_unsafe_block_if_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_switch_expression_type.or"),
        low_level_final_unsafe_block_switch_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_switch_expression_success.or"),
        low_level_final_unsafe_block_switch_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_unsafe_block_expression_type.or"),
        low_level_final_if_unsafe_block_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_unsafe_block_expression_success.or"),
        low_level_final_if_unsafe_block_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_unsafe_block_expression_type.or"),
        low_level_final_switch_unsafe_block_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_unsafe_block_expression_success.or"),
        low_level_final_switch_unsafe_block_read_success_lines(intrinsic)
    );
}

void assert_cli_final_container_return_parse_cases(
    std::filesystem::path const& executable,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_return_type.or"),
        low_level_final_if_lines(
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "return " + low_level_read_call(intrinsic, "left"),
            "return 1 as UInt32"
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_return_success.or"),
        low_level_final_if_lines(
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "return " + low_level_read_call(intrinsic, "left"),
            "return " + low_level_read_call(intrinsic, "right")
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_return_type.or"),
        low_level_final_switch_lines(
            "unsafe function read_word(flag: Bool, left: Pointer<Byte>) -> UInt32",
            "return " + low_level_read_call(intrinsic, "left"),
            "return 1 as UInt32"
        ),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_return_success.or"),
        low_level_final_switch_lines(
            "unsafe function read_byte(flag: Bool, left: Pointer<Byte>, right: Pointer<Byte>) -> Byte",
            "return " + low_level_read_call(intrinsic, "left"),
            "return " + low_level_read_call(intrinsic, "right")
        )
    );
}

void assert_cli_final_ternary_low_level_parse_cases(
    std::filesystem::path const& executable,
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_ternary_expression_type.or"),
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
        std::filesystem::temp_directory_path() / (prefix + "_final_ternary_expression_success.or"),
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
        std::filesystem::temp_directory_path() / (prefix + "_final_ternary_return_type.or"),
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
        std::filesystem::temp_directory_path() / (prefix + "_final_ternary_return_success.or"),
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
    std::string_view intrinsic
) {
    auto result_mismatch_message = std::string(intrinsic) +
                                   " result type 'Byte' does not match function return type 'UInt32'";
    auto prefix = std::string("orison_cli_") + std::string(intrinsic);
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_ternary_type.or"),
        low_level_final_unsafe_block_ternary_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_unsafe_block_final_ternary_success.or"),
        low_level_final_unsafe_block_ternary_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_ternary_type.or"),
        low_level_final_if_ternary_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_if_ternary_success.or"),
        low_level_final_if_ternary_read_success_lines(intrinsic)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_ternary_type.or"),
        low_level_final_switch_ternary_read_mismatch_lines(intrinsic),
        result_mismatch_message
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / (prefix + "_final_switch_ternary_success.or"),
        low_level_final_switch_ternary_read_success_lines(intrinsic)
    );
}

}  // namespace

int main() {
    auto original_temp_root = std::filesystem::temp_directory_path();
    auto smoke_temp_root =
        original_temp_root / ("orisonc_cli_smoke_" + std::to_string(static_cast<long long>(::getpid())));
    std::filesystem::remove_all(smoke_temp_root);
    std::filesystem::create_directories(smoke_temp_root);
    auto smoke_temp_root_text = smoke_temp_root.string();
    assert(::setenv("TMPDIR", smoke_temp_root_text.c_str(), 1) == 0);

    auto path = std::filesystem::temp_directory_path() / "orison_cli_smoke.or";
    {
        std::ofstream output(path);
        output << "package demo.cli\n";
        output << "const UART0_BASE: Address = 0x4000_1000\n";
        output << "import\n";
        output << "    Logger as Log from diagnostics.logger\n";
        output << "package foreign \"c\" library \"m\"\n";
        output << "    function sin(x: Float64) -> Float64\n";
        output << "public foreign \"c\" as \"device_init\"\n";
        output << "function initialize_device() -> Int32\n";
        output << "    return 0\n";
        output << "public type Port = UInt16\n";
        output << "async function fetch(url: Text) -> Outcome<Text, IOError>\n";
        output << "    let request_task = task\n";
        output << "        request(url)\n";
        output << "    return await request_task\n";
        output << "public function parallel_sum(data: shared View<Int64>) -> Int64\n";
        output << "    let worker = thread\n";
        output << "        sum(data)\n";
        output << "    return worker.join()\n";
        output << "unsafe function read_word(addr: Address) -> UInt32\n";
        output << "    return raw_read(addr)\n";
        output << "choice MaybeText\n";
        output << "    Some(value: Text)\n";
        output << "    Empty\n";
        output << "public interface Reader\n";
        output << "    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<Int32, ParseError>\n";
        output << "implements Reader for FileReader\n";
        output << "    function read(this: exclusive This, into: exclusive View<Byte>) -> Outcome<Int32, ParseError>\n";
        output << "        return into.length()\n";
        output << "extend FileReader\n";
        output << "    public function reset(this: exclusive This) -> Unit\n";
        output << "        return input.length()\n";
        output << "package function main<R>(input: shared.View<Byte>, state: MaybeText, reader: exclusive R) -> Outcome<Int32, ParseError>\n";
        output << "where R: Reader\n";
        output << "    switch state\n";
        output << "        Some(value) => return value.length()\n";
        output << "        Empty => return input.length()\n";
        output << "    return input.read(2)\n";
    }

    auto executable = std::filesystem::current_path().parent_path() / "tools" / "orisonc" / "orisonc";
    auto command = executable.string() + " --parse " + path.string();
    auto output = read_command_output(command);

    assert(output.find("parsed ") != std::string::npos);
    assert(output.find("package demo.cli") != std::string::npos);
    assert(output.find("top-level declarations: 12") != std::string::npos);
    assert(output.find("imports: 1") != std::string::npos);
    assert(output.find("foreign imports: 1") != std::string::npos);
    assert(output.find("foreign exports: 1") != std::string::npos);
    assert(output.find("constants: 1") != std::string::npos);
    assert(output.find("type aliases: 1") != std::string::npos);
    assert(output.find("first import from: diagnostics.logger") != std::string::npos);
    assert(output.find("first foreign import abi: \"c\"") != std::string::npos);
    assert(output.find("first foreign import library: \"m\"") != std::string::npos);
    assert(output.find("first foreign import functions: 1") != std::string::npos);
    assert(output.find("first foreign export abi: \"c\"") != std::string::npos);
    assert(output.find("first foreign export symbol: \"device_init\"") != std::string::npos);
    assert(output.find("first foreign export function: initialize_device") != std::string::npos);
    assert(output.find("first constant type: Address") != std::string::npos);
    assert(output.find("first constant initializer: 0x4000_1000") != std::string::npos);
    assert(output.find("first type alias visibility: public") != std::string::npos);
    assert(output.find("first type alias target: UInt16") != std::string::npos);
    assert(output.find("records: 0") != std::string::npos);
    assert(output.find("choices: 1") != std::string::npos);
    assert(output.find("interfaces: 1") != std::string::npos);
    assert(output.find("implementations: 1") != std::string::npos);
    assert(output.find("extensions: 1") != std::string::npos);
    assert(output.find("first interface visibility: public") != std::string::npos);
    assert(output.find("first interface methods: 1") != std::string::npos);
    assert(output.find("first implementation interface: Reader") != std::string::npos);
    assert(output.find("first implementation receiver: FileReader") != std::string::npos);
    assert(output.find("first implementation methods: 1") != std::string::npos);
    assert(output.find("first extension receiver: FileReader") != std::string::npos);
    assert(output.find("first extension methods: 1") != std::string::npos);
    assert(output.find("first extension method visibility: public") != std::string::npos);
    assert(output.find("functions: 4") != std::string::npos);
    assert(output.find("first function visibility: package") != std::string::npos);
    assert(output.find("first function async: true") != std::string::npos);
    assert(output.find("first function unsafe: false") != std::string::npos);
    assert(output.find("function parameters: 1") != std::string::npos);
    assert(output.find("function return type: Outcome<Text, IOError>") != std::string::npos);
    assert(output.find("function where constraints: 0") != std::string::npos);
    assert(output.find("function body statements: 2") != std::string::npos);
    assert(output.find("first statement kind: let") != std::string::npos);
    assert(output.find("first statement expression: task { request(url) }") != std::string::npos);
    assert(output.find("first statement nested count: 0") != std::string::npos);
    assert(output.find("first statement alternate count: 0") != std::string::npos);
    assert(output.find("first statement switch cases: 0") != std::string::npos);

    auto emit_path = std::filesystem::temp_directory_path() / "orison_cli_emit_llvm.or";
    {
        std::ofstream emit_source(emit_path);
        emit_source << "package demo.emit\n";
        emit_source << "function main() -> UInt32\n";
        emit_source << "    42 as UInt32\n";
    }
    auto emit_output = read_command_output(executable.string() + " --emit-llvm " + emit_path.string());
    assert(emit_output.find("; package demo.emit") != std::string::npos);
    assert(emit_output.find("define i32 @main()") != std::string::npos);
    assert(emit_output.find("ret i32 42") != std::string::npos);

    auto local_record_assignment_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_field_assignment.or";
    auto local_record_assignment_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + local_record_assignment_emit_path.string());
    assert(
        local_record_assignment_emit_output.find(
            "getelementptr %record.Log, ptr %log.addr, i32 0, i32 0"
        ) != std::string::npos
    );
    assert(
        local_record_assignment_emit_output.find(
            "getelementptr [2 x %record.UartRegisters], ptr %tmp"
        ) != std::string::npos
    );
    assert(
        local_record_assignment_emit_output.find(
            "getelementptr %record.UartRegisters, ptr %tmp"
        ) != std::string::npos
    );
    assert(local_record_assignment_emit_output.find("store i32 8, ptr %tmp") != std::string::npos);

    auto pointer_record_assignment_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "pointer_record_field_assignment.or";
    auto pointer_record_assignment_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + pointer_record_assignment_emit_path.string());
    assert(
        pointer_record_assignment_emit_output.find(
            "getelementptr %record.Log, ptr %log, i32 0, i32 0"
        ) != std::string::npos
    );
    assert(
        pointer_record_assignment_emit_output.find(
            "getelementptr [2 x %record.UartRegisters], ptr %tmp"
        ) != std::string::npos
    );
    assert(
        pointer_record_assignment_emit_output.find(
            "getelementptr %record.UartRegisters, ptr %tmp"
        ) != std::string::npos
    );
    assert(pointer_record_assignment_emit_output.find("store i32 8, ptr %tmp") != std::string::npos);

    auto inferred_record_array_let_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_record_array_let.or";
    auto inferred_record_array_let_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + inferred_record_array_let_emit_path.string());
    assert(
        inferred_record_array_let_emit_output.find(
            "alloca %record.Packet"
        ) != std::string::npos
    );
    assert(
        inferred_record_array_let_emit_output.find(
            "getelementptr %record.Packet"
        ) != std::string::npos
    );
    assert(inferred_record_array_let_emit_output.find("getelementptr [4 x i8]") != std::string::npos);
    assert(inferred_record_array_let_emit_output.find("load i8, ptr") != std::string::npos);

    auto inferred_array_record_let_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_array_record_let.or";
    auto inferred_array_record_let_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + inferred_array_record_let_emit_path.string());
    assert(
        inferred_array_record_let_emit_output.find(
            "alloca [2 x %record.Entry]"
        ) != std::string::npos
    );
    assert(
        inferred_array_record_let_emit_output.find(
            "getelementptr [2 x %record.Entry]"
        ) != std::string::npos
    );
    assert(inferred_array_record_let_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(inferred_array_record_let_emit_output.find("load i32, ptr") != std::string::npos);

    auto inferred_nested_mixed_let_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_nested_mixed_let.or";
    auto inferred_nested_mixed_let_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + inferred_nested_mixed_let_emit_path.string());
    assert(
        inferred_nested_mixed_let_emit_output.find(
            "alloca %record.Page"
        ) != std::string::npos
    );
    assert(
        inferred_nested_mixed_let_emit_output.find(
            "getelementptr %record.Page"
        ) != std::string::npos
    );
    assert(
        inferred_nested_mixed_let_emit_output.find(
            "getelementptr [2 x %record.Entry]"
        ) != std::string::npos
    );
    assert(inferred_nested_mixed_let_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(inferred_nested_mixed_let_emit_output.find("load i32, ptr") != std::string::npos);

    auto branch_inferred_aggregate_let_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_inferred_aggregate_let.or";
    auto branch_inferred_aggregate_let_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + branch_inferred_aggregate_let_emit_path.string());
    assert(branch_inferred_aggregate_let_emit_output.find("if.then.") != std::string::npos);
    assert(branch_inferred_aggregate_let_emit_output.find("if.else.") != std::string::npos);
    assert(branch_inferred_aggregate_let_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(branch_inferred_aggregate_let_emit_output.find("getelementptr %record.Page") != std::string::npos);
    assert(branch_inferred_aggregate_let_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(branch_inferred_aggregate_let_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(branch_inferred_aggregate_let_emit_output.find("load i32, ptr") != std::string::npos);
    assert(branch_inferred_aggregate_let_emit_output.find(" = phi i32 ") != std::string::npos);

    auto inferred_aggregate_reassignment_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_aggregate_reassignment.or";
    auto inferred_aggregate_reassignment_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + inferred_aggregate_reassignment_emit_path.string()
    );
    assert(inferred_aggregate_reassignment_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(inferred_aggregate_reassignment_emit_output.find("store %record.Page") != std::string::npos);
    assert(
        inferred_aggregate_reassignment_emit_output.find("getelementptr %record.Page, ptr %page.addr") !=
        std::string::npos
    );
    assert(
        inferred_aggregate_reassignment_emit_output.find("getelementptr [2 x %record.Entry]") !=
        std::string::npos
    );
    assert(inferred_aggregate_reassignment_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(inferred_aggregate_reassignment_emit_output.find("load i32, ptr") != std::string::npos);

    auto branch_aggregate_reassignment_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_aggregate_reassignment.or";
    auto branch_aggregate_reassignment_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + branch_aggregate_reassignment_emit_path.string()
    );
    assert(branch_aggregate_reassignment_emit_output.find("if.then.") != std::string::npos);
    assert(branch_aggregate_reassignment_emit_output.find("if.else.") != std::string::npos);
    assert(branch_aggregate_reassignment_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(branch_aggregate_reassignment_emit_output.find("store %record.Page") != std::string::npos);
    assert(
        branch_aggregate_reassignment_emit_output.find("getelementptr %record.Page, ptr %page.addr") !=
        std::string::npos
    );
    assert(branch_aggregate_reassignment_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(branch_aggregate_reassignment_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(branch_aggregate_reassignment_emit_output.find("load i32, ptr") != std::string::npos);

    auto switch_aggregate_reassignment_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_switch_aggregate_reassignment.or";
    auto switch_aggregate_reassignment_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + switch_aggregate_reassignment_emit_path.string()
    );
    assert(switch_aggregate_reassignment_emit_output.find("switch i1") != std::string::npos);
    assert(switch_aggregate_reassignment_emit_output.find("switch.case.") != std::string::npos);
    assert(switch_aggregate_reassignment_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(switch_aggregate_reassignment_emit_output.find("store %record.Page") != std::string::npos);
    assert(
        switch_aggregate_reassignment_emit_output.find("getelementptr %record.Page, ptr %page.addr") !=
        std::string::npos
    );
    assert(switch_aggregate_reassignment_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(switch_aggregate_reassignment_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(switch_aggregate_reassignment_emit_output.find("load i32, ptr") != std::string::npos);

    auto mutable_aggregate_path_read_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_mutable_aggregate_path_read.or";
    auto mutable_aggregate_path_read_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + mutable_aggregate_path_read_emit_path.string()
    );
    assert(
        mutable_aggregate_path_read_emit_output.find("define i32 @selected_status(i64 %index)") !=
        std::string::npos
    );
    assert(mutable_aggregate_path_read_emit_output.find("define i32 @selected_code()") != std::string::npos);
    assert(mutable_aggregate_path_read_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(
        mutable_aggregate_path_read_emit_output.find("getelementptr %record.Page, ptr %page.addr") !=
        std::string::npos
    );
    assert(
        mutable_aggregate_path_read_emit_output.find(
            "getelementptr [2 x %record.Entry], ptr"
        ) != std::string::npos
    );
    assert(mutable_aggregate_path_read_emit_output.find(", i64 0, i64 %index") != std::string::npos);
    assert(mutable_aggregate_path_read_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(mutable_aggregate_path_read_emit_output.find("load i32, ptr") != std::string::npos);
    assert(mutable_aggregate_path_read_emit_output.find("ret i32") != std::string::npos);

    auto branch_aggregate_field_assignment_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_aggregate_field_assignment.or";
    auto branch_aggregate_field_assignment_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + branch_aggregate_field_assignment_emit_path.string()
    );
    assert(branch_aggregate_field_assignment_emit_output.find("if.then.") != std::string::npos);
    assert(branch_aggregate_field_assignment_emit_output.find("if.else.") != std::string::npos);
    assert(branch_aggregate_field_assignment_emit_output.find("getelementptr %record.Page") != std::string::npos);
    assert(branch_aggregate_field_assignment_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(branch_aggregate_field_assignment_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(branch_aggregate_field_assignment_emit_output.find("store i32 13") != std::string::npos);
    assert(branch_aggregate_field_assignment_emit_output.find("store i32 17") != std::string::npos);
    assert(branch_aggregate_field_assignment_emit_output.find("load i32, ptr") != std::string::npos);

    auto switch_aggregate_field_assignment_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_switch_aggregate_field_assignment.or";
    auto switch_aggregate_field_assignment_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + switch_aggregate_field_assignment_emit_path.string()
    );
    assert(switch_aggregate_field_assignment_emit_output.find("switch i1") != std::string::npos);
    assert(switch_aggregate_field_assignment_emit_output.find("switch.case.") != std::string::npos);
    assert(switch_aggregate_field_assignment_emit_output.find("getelementptr %record.Page") != std::string::npos);
    assert(switch_aggregate_field_assignment_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(switch_aggregate_field_assignment_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(switch_aggregate_field_assignment_emit_output.find("store i32 19") != std::string::npos);
    assert(switch_aggregate_field_assignment_emit_output.find("store i32 23") != std::string::npos);
    assert(switch_aggregate_field_assignment_emit_output.find("load i32, ptr") != std::string::npos);

    auto branch_nested_array_assignment_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_nested_array_assignment.or";
    auto branch_nested_array_assignment_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + branch_nested_array_assignment_emit_path.string()
    );
    assert(branch_nested_array_assignment_emit_output.find("if.then.") != std::string::npos);
    assert(branch_nested_array_assignment_emit_output.find("if.else.") != std::string::npos);
    assert(branch_nested_array_assignment_emit_output.find("getelementptr %record.Matrix") != std::string::npos);
    assert(branch_nested_array_assignment_emit_output.find("getelementptr [2 x [2 x i32]]") != std::string::npos);
    assert(branch_nested_array_assignment_emit_output.find("getelementptr [2 x i32]") != std::string::npos);
    assert(branch_nested_array_assignment_emit_output.find("store i32 31") != std::string::npos);
    assert(branch_nested_array_assignment_emit_output.find("store i32 37") != std::string::npos);
    assert(branch_nested_array_assignment_emit_output.find("load i32, ptr") != std::string::npos);

    auto switch_nested_array_assignment_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_switch_nested_array_assignment.or";
    auto switch_nested_array_assignment_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + switch_nested_array_assignment_emit_path.string()
    );
    assert(switch_nested_array_assignment_emit_output.find("switch i1") != std::string::npos);
    assert(switch_nested_array_assignment_emit_output.find("switch.case.") != std::string::npos);
    assert(switch_nested_array_assignment_emit_output.find("getelementptr %record.Matrix") != std::string::npos);
    assert(switch_nested_array_assignment_emit_output.find("getelementptr [2 x [2 x i32]]") != std::string::npos);
    assert(switch_nested_array_assignment_emit_output.find("getelementptr [2 x i32]") != std::string::npos);
    assert(switch_nested_array_assignment_emit_output.find("store i32 41") != std::string::npos);
    assert(switch_nested_array_assignment_emit_output.find("store i32 43") != std::string::npos);
    assert(switch_nested_array_assignment_emit_output.find("load i32, ptr") != std::string::npos);

    auto helper_aggregate_access_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_helper_aggregate_access.or";
    auto helper_aggregate_access_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + helper_aggregate_access_emit_path.string());
    assert(helper_aggregate_access_emit_output.find("call %record.Page @make_page()") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("call [2 x i32] @make_values()") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("alloca [2 x i32]") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("store %record.Page") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("store [2 x i32]") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("getelementptr %record.Page") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("getelementptr [2 x i32]") != std::string::npos);
    assert(helper_aggregate_access_emit_output.find("load i32, ptr") != std::string::npos);

    auto aggregate_parameter_access_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_aggregate_parameter_access.or";
    auto aggregate_parameter_access_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + aggregate_parameter_access_emit_path.string());
    assert(
        aggregate_parameter_access_emit_output.find("define i32 @pick_status(%record.Page %page)") !=
        std::string::npos
    );
    assert(
        aggregate_parameter_access_emit_output.find("define i32 @pick_value([2 x i32] %values)") !=
        std::string::npos
    );
    assert(
        aggregate_parameter_access_emit_output.find(
            "define i32 @method.Reader.status_from(%record.Reader %this, %record.Page %page)"
        ) !=
        std::string::npos
    );
    assert(
        aggregate_parameter_access_emit_output.find(
            "define i32 @method.Reader.value_from(%record.Reader %this, [2 x i32] %values)"
        ) !=
        std::string::npos
    );
    assert(
        aggregate_parameter_access_emit_output.find("call i32 @pick_status(%record.Page") !=
        std::string::npos
    );
    assert(aggregate_parameter_access_emit_output.find("call i32 @pick_value([2 x i32]") != std::string::npos);
    assert(
        aggregate_parameter_access_emit_output.find("call i32 @method.Reader.status_from(%record.Reader") !=
        std::string::npos
    );
    assert(
        aggregate_parameter_access_emit_output.find("call i32 @method.Reader.value_from(%record.Reader") !=
        std::string::npos
    );
    assert(aggregate_parameter_access_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(aggregate_parameter_access_emit_output.find("alloca [2 x i32]") != std::string::npos);
    assert(aggregate_parameter_access_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(aggregate_parameter_access_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(aggregate_parameter_access_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(aggregate_parameter_access_emit_output.find("getelementptr %record.Reader, ptr %this.addr") != std::string::npos);
    assert(aggregate_parameter_access_emit_output.find("getelementptr [2 x i32], ptr %values.addr") != std::string::npos);
    assert(aggregate_parameter_access_emit_output.find("load i32, ptr") != std::string::npos);
    assert(aggregate_parameter_access_emit_output.find("add i32") != std::string::npos);

    auto call_argument_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_call_argument_aggregate_scalar.or";
    auto call_argument_aggregate_scalar_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + call_argument_aggregate_scalar_emit_path.string()
    );
    assert(
        call_argument_aggregate_scalar_emit_output.find("define i32 @combine(i32 %left, i32 %right)") !=
        std::string::npos
    );
    assert(
        call_argument_aggregate_scalar_emit_output.find(
            "define i32 @method.Sink.mix(%record.Sink %this, i32 %left, i32 %right)"
        ) != std::string::npos
    );
    assert(call_argument_aggregate_scalar_emit_output.find("call i32 @combine(i32") != std::string::npos);
    assert(call_argument_aggregate_scalar_emit_output.find("call i32 @method.Sink.mix(%record.Sink") != std::string::npos);
    assert(call_argument_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(call_argument_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(call_argument_aggregate_scalar_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(call_argument_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(call_argument_aggregate_scalar_emit_output.find("add i32") != std::string::npos);

    auto ffi_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "ffi_aggregate_scalar_parameters.or";
    auto ffi_aggregate_scalar_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + ffi_aggregate_scalar_emit_path.string());
    assert(ffi_aggregate_scalar_emit_output.find("declare i32 @printf(ptr, ...)") != std::string::npos);
    assert(ffi_aggregate_scalar_emit_output.find("define i32 @print_entry(%record.Page %page)") != std::string::npos);
    assert(ffi_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(ffi_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(ffi_aggregate_scalar_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(ffi_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(ffi_aggregate_scalar_emit_output.find("call i32 (ptr, ...) @printf(ptr @.str.0, i32") != std::string::npos);

    auto return_container_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_return_container_aggregate_scalar.or";
    auto return_container_aggregate_scalar_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + return_container_aggregate_scalar_emit_path.string()
    );
    assert(
        return_container_aggregate_scalar_emit_output.find("define %record.Pair @make_pair(%record.Page %page)") !=
        std::string::npos
    );
    assert(
        return_container_aggregate_scalar_emit_output.find("define [2 x i32] @make_values(%record.Page %page)") !=
        std::string::npos
    );
    assert(return_container_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(return_container_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(return_container_aggregate_scalar_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(return_container_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(return_container_aggregate_scalar_emit_output.find("insertvalue %record.Pair") != std::string::npos);
    assert(return_container_aggregate_scalar_emit_output.find("insertvalue [2 x i32]") != std::string::npos);
    assert(return_container_aggregate_scalar_emit_output.find("ret %record.Pair") != std::string::npos);
    assert(return_container_aggregate_scalar_emit_output.find("ret [2 x i32]") != std::string::npos);

    auto nested_return_container_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_nested_return_container_aggregate_scalar.or";
    auto nested_return_container_aggregate_scalar_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + nested_return_container_aggregate_scalar_emit_path.string()
    );
    assert(
        nested_return_container_aggregate_scalar_emit_output.find(
            "define %record.Snapshot @make_snapshot(%record.Page %page)"
        ) != std::string::npos
    );
    assert(
        nested_return_container_aggregate_scalar_emit_output.find(
            "define [2 x [2 x i32]] @make_matrix(%record.Page %page)"
        ) != std::string::npos
    );
    assert(nested_return_container_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(nested_return_container_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(nested_return_container_aggregate_scalar_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(nested_return_container_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(nested_return_container_aggregate_scalar_emit_output.find("insertvalue %record.Snapshot") != std::string::npos);
    assert(nested_return_container_aggregate_scalar_emit_output.find("insertvalue [2 x i32]") != std::string::npos);
    assert(nested_return_container_aggregate_scalar_emit_output.find("insertvalue [2 x [2 x i32]]") != std::string::npos);
    assert(nested_return_container_aggregate_scalar_emit_output.find("ret %record.Snapshot") != std::string::npos);
    assert(nested_return_container_aggregate_scalar_emit_output.find("ret [2 x [2 x i32]]") != std::string::npos);

    auto branch_return_container_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_return_container_aggregate_scalar.or";
    auto branch_return_container_aggregate_scalar_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + branch_return_container_aggregate_scalar_emit_path.string()
    );
    assert(
        branch_return_container_aggregate_scalar_emit_output.find(
            "define %record.Pair @choose_pair(i1 %flag, %record.Page %page)"
        ) != std::string::npos
    );
    assert(
        branch_return_container_aggregate_scalar_emit_output.find(
            "define [2 x i32] @choose_values(i32 %selector, %record.Page %page)"
        ) != std::string::npos
    );
    assert(branch_return_container_aggregate_scalar_emit_output.find("if.then.") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("if.else.") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("switch i32") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("switch.case.") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("insertvalue %record.Pair") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("insertvalue [2 x i32]") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("phi %record.Pair") != std::string::npos);
    assert(branch_return_container_aggregate_scalar_emit_output.find("phi [2 x i32]") != std::string::npos);

    auto loop_return_container_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_loop_return_container_aggregate_scalar.or";
    auto loop_return_container_aggregate_scalar_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + loop_return_container_aggregate_scalar_emit_path.string()
    );
    assert(
        loop_return_container_aggregate_scalar_emit_output.find(
            "define %record.Pair @make_pair_while(%record.Page %page)"
        ) != std::string::npos
    );
    assert(
        loop_return_container_aggregate_scalar_emit_output.find(
            "define [2 x i32] @make_values_for(%record.Page %page)"
        ) != std::string::npos
    );
    assert(loop_return_container_aggregate_scalar_emit_output.find("while.condition.") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("while.body.") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("while.exit.") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("for.iteration.") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("for.exit.") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("load i32, ptr %total.addr") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("%entry.addr = alloca %record.Entry") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("%entry.addr.1 = alloca %record.Entry") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("store %record.Entry") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("getelementptr %record.Entry, ptr %entry.addr") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("insertvalue %record.Pair") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("insertvalue [2 x i32]") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("ret %record.Pair") != std::string::npos);
    assert(loop_return_container_aggregate_scalar_emit_output.find("ret [2 x i32]") != std::string::npos);

    auto control_flow_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_control_flow_aggregate_scalar.or";
    auto control_flow_aggregate_scalar_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + control_flow_aggregate_scalar_emit_path.string()
    );
    assert(control_flow_aggregate_scalar_emit_output.find("define i32 @choose_if") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("define i32 @choose_switch") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("if.then.") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("if.else.") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("switch i32") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("switch.case.") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("getelementptr [2 x i32], ptr %values.addr") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find(" = phi i32 ") != std::string::npos);
    assert(control_flow_aggregate_scalar_emit_output.find("add i32") != std::string::npos);

    auto loop_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_loop_aggregate_scalar.or";
    auto loop_aggregate_scalar_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + loop_aggregate_scalar_emit_path.string());
    assert(loop_aggregate_scalar_emit_output.find("define i32 @sum_while") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("define i32 @sum_for") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("while.condition.") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("while.body.") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("for.iteration.") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("getelementptr [2 x i32], ptr %values.addr") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("store i32") != std::string::npos);
    assert(loop_aggregate_scalar_emit_output.find("add i32") != std::string::npos);

    auto guard_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_guard_aggregate_scalar.or";
    auto guard_aggregate_scalar_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + guard_aggregate_scalar_emit_path.string());
    assert(guard_aggregate_scalar_emit_output.find("define i32 @guarded_total") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("guard.failure.") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("guard.merge.") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("getelementptr [2 x i32], ptr %values.addr") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("icmp ugt i32") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("ret i32") != std::string::npos);
    assert(guard_aggregate_scalar_emit_output.find("add i32") != std::string::npos);

    auto defer_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_defer_aggregate_scalar.or";
    auto defer_aggregate_scalar_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + defer_aggregate_scalar_emit_path.string());
    assert(defer_aggregate_scalar_emit_output.find("define i32 @cleanup_total") != std::string::npos);
    auto first_defer_aggregate_scalar_call =
        defer_aggregate_scalar_emit_output.find("call void @observe(i32");
    assert(first_defer_aggregate_scalar_call != std::string::npos);
    assert(
        defer_aggregate_scalar_emit_output.find(
            "call void @observe(i32",
            first_defer_aggregate_scalar_call + 1
        ) !=
        std::string::npos
    );
    assert(defer_aggregate_scalar_emit_output.find("getelementptr %record.Page, ptr %page.addr") != std::string::npos);
    assert(defer_aggregate_scalar_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(defer_aggregate_scalar_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(defer_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(defer_aggregate_scalar_emit_output.find("getelementptr [2 x i32], ptr %values.addr") != std::string::npos);
    assert(defer_aggregate_scalar_emit_output.find("ret i32") != std::string::npos);
    assert(defer_aggregate_scalar_emit_output.find("add i32") != std::string::npos);

    auto unsafe_aggregate_scalar_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_unsafe_aggregate_scalar.or";
    auto unsafe_aggregate_scalar_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + unsafe_aggregate_scalar_emit_path.string());
    assert(unsafe_aggregate_scalar_emit_output.find("define i32 @read_status") != std::string::npos);
    assert(unsafe_aggregate_scalar_emit_output.find("define i32 @read_control") != std::string::npos);
    assert(unsafe_aggregate_scalar_emit_output.find("define i32 @combined_block") != std::string::npos);
    assert(unsafe_aggregate_scalar_emit_output.find("getelementptr %record.Log") != std::string::npos);
    assert(unsafe_aggregate_scalar_emit_output.find("getelementptr [2 x %record.UartRegisters]") != std::string::npos);
    assert(unsafe_aggregate_scalar_emit_output.find("getelementptr %record.UartRegisters") != std::string::npos);
    assert(unsafe_aggregate_scalar_emit_output.find("load i32, ptr") != std::string::npos);
    assert(unsafe_aggregate_scalar_emit_output.find("add i32") != std::string::npos);

    auto method_aggregate_access_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_method_aggregate_access.or";
    auto method_aggregate_access_emit_output =
        read_command_output(executable.string() + " --emit-llvm " + method_aggregate_access_emit_path.string());
    assert(method_aggregate_access_emit_output.find("call %record.Page @method.UInt32.page") != std::string::npos);
    assert(method_aggregate_access_emit_output.find("call [2 x i32] @method.UInt32.values") != std::string::npos);
    assert(method_aggregate_access_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(method_aggregate_access_emit_output.find("alloca [2 x i32]") != std::string::npos);
    assert(method_aggregate_access_emit_output.find("getelementptr %record.Page") != std::string::npos);
    assert(method_aggregate_access_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(method_aggregate_access_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(method_aggregate_access_emit_output.find("getelementptr [2 x i32]") != std::string::npos);
    assert(method_aggregate_access_emit_output.find("load i32, ptr") != std::string::npos);

    auto record_method_aggregate_access_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_method_aggregate_access.or";
    auto record_method_aggregate_access_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + record_method_aggregate_access_emit_path.string()
    );
    assert(
        record_method_aggregate_access_emit_output.find("call %record.Page @method.Wrapper.page(%record.Wrapper") !=
        std::string::npos
    );
    assert(
        record_method_aggregate_access_emit_output.find("call [2 x i32] @method.Wrapper.values(%record.Wrapper") !=
        std::string::npos
    );
    assert(record_method_aggregate_access_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(record_method_aggregate_access_emit_output.find("alloca [2 x i32]") != std::string::npos);
    assert(record_method_aggregate_access_emit_output.find("getelementptr %record.Page") != std::string::npos);
    assert(record_method_aggregate_access_emit_output.find("getelementptr [2 x %record.Entry]") != std::string::npos);
    assert(record_method_aggregate_access_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(record_method_aggregate_access_emit_output.find("getelementptr [2 x i32]") != std::string::npos);
    assert(record_method_aggregate_access_emit_output.find("load i32, ptr") != std::string::npos);

    auto member_receiver_method_aggregate_access_emit_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" /
        "local_member_receiver_method_aggregate_access.or";
    auto member_receiver_method_aggregate_access_emit_output = read_command_output(
        executable.string() + " --emit-llvm " + member_receiver_method_aggregate_access_emit_path.string()
    );
    assert(
        member_receiver_method_aggregate_access_emit_output.find(
            "call %record.Page @method.Bucket.page(%record.Bucket"
        ) !=
        std::string::npos
    );
    assert(
        member_receiver_method_aggregate_access_emit_output.find(
            "call [2 x i32] @method.Bucket.values(%record.Bucket"
        ) !=
        std::string::npos
    );
    assert(
        member_receiver_method_aggregate_access_emit_output.find("getelementptr %record.Wrapper, ptr %wrapper.addr") !=
        std::string::npos
    );
    assert(
        member_receiver_method_aggregate_access_emit_output.find("getelementptr [2 x %record.Bucket]") !=
        std::string::npos
    );
    assert(member_receiver_method_aggregate_access_emit_output.find("load %record.Bucket, ptr") != std::string::npos);
    assert(member_receiver_method_aggregate_access_emit_output.find("alloca %record.Page") != std::string::npos);
    assert(member_receiver_method_aggregate_access_emit_output.find("alloca [2 x i32]") != std::string::npos);
    assert(member_receiver_method_aggregate_access_emit_output.find("getelementptr %record.Page") != std::string::npos);
    assert(
        member_receiver_method_aggregate_access_emit_output.find("getelementptr [2 x %record.Entry]") !=
        std::string::npos
    );
    assert(member_receiver_method_aggregate_access_emit_output.find("getelementptr %record.Entry") != std::string::npos);
    assert(member_receiver_method_aggregate_access_emit_output.find("getelementptr [2 x i32]") != std::string::npos);
    assert(member_receiver_method_aggregate_access_emit_output.find("load i32, ptr") != std::string::npos);

    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_emit_scalar_member_assignment.or",
        cli_module_lines(
            {
                "unsafe function main() -> Unit",
                "    var total: UInt32 = 0 as UInt32",
                "    total.status = 1 as UInt32",
                "    return",
            }
        ),
        "lowering aggregate assignment member target is unsupported"
    );
    assert_cli_emit_llvm_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_emit_scalar_index_assignment.or",
        cli_module_lines(
            {
                "unsafe function main(index: UInt64) -> Unit",
                "    var total: UInt32 = 0 as UInt32",
                "    total[index] = 1 as UInt32",
                "    return",
            }
        ),
        "lowering aggregate assignment index target is unsupported"
    );

    auto object_path = std::filesystem::temp_directory_path() / "orison_cli_emit_object.o";
    auto object_command =
        executable.string() + " --emit-object " + emit_path.string() + " -o " + object_path.string();
    assert(read_command_output(object_command).empty());
    assert(std::filesystem::file_size(object_path) > 0);

    auto local_record_assignment_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_local_record_field_assignment.o";
    auto local_record_assignment_object_command =
        executable.string() + " --emit-object " + local_record_assignment_emit_path.string() + " -o " +
        local_record_assignment_object_path.string();
    assert(read_command_output(local_record_assignment_object_command).empty());
    assert(std::filesystem::file_size(local_record_assignment_object_path) > 0);

    auto pointer_record_assignment_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_pointer_record_field_assignment.o";
    auto pointer_record_assignment_object_command =
        executable.string() + " --emit-object " + pointer_record_assignment_emit_path.string() + " -o " +
        pointer_record_assignment_object_path.string();
    assert(read_command_output(pointer_record_assignment_object_command).empty());
    assert(std::filesystem::file_size(pointer_record_assignment_object_path) > 0);

    auto inferred_record_array_let_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_inferred_record_array_let.o";
    auto inferred_record_array_let_object_command =
        executable.string() + " --emit-object " + inferred_record_array_let_emit_path.string() + " -o " +
        inferred_record_array_let_object_path.string();
    assert(read_command_output(inferred_record_array_let_object_command).empty());
    assert(std::filesystem::file_size(inferred_record_array_let_object_path) > 0);

    auto inferred_array_record_let_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_inferred_array_record_let.o";
    auto inferred_array_record_let_object_command =
        executable.string() + " --emit-object " + inferred_array_record_let_emit_path.string() + " -o " +
        inferred_array_record_let_object_path.string();
    assert(read_command_output(inferred_array_record_let_object_command).empty());
    assert(std::filesystem::file_size(inferred_array_record_let_object_path) > 0);

    auto inferred_nested_mixed_let_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_inferred_nested_mixed_let.o";
    auto inferred_nested_mixed_let_object_command =
        executable.string() + " --emit-object " + inferred_nested_mixed_let_emit_path.string() + " -o " +
        inferred_nested_mixed_let_object_path.string();
    assert(read_command_output(inferred_nested_mixed_let_object_command).empty());
    assert(std::filesystem::file_size(inferred_nested_mixed_let_object_path) > 0);

    auto branch_inferred_aggregate_let_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_inferred_aggregate_let.o";
    auto branch_inferred_aggregate_let_object_command =
        executable.string() + " --emit-object " + branch_inferred_aggregate_let_emit_path.string() + " -o " +
        branch_inferred_aggregate_let_object_path.string();
    assert(read_command_output(branch_inferred_aggregate_let_object_command).empty());
    assert(std::filesystem::file_size(branch_inferred_aggregate_let_object_path) > 0);

    auto inferred_aggregate_reassignment_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_inferred_aggregate_reassignment.o";
    auto inferred_aggregate_reassignment_object_command =
        executable.string() + " --emit-object " + inferred_aggregate_reassignment_emit_path.string() + " -o " +
        inferred_aggregate_reassignment_object_path.string();
    assert(read_command_output(inferred_aggregate_reassignment_object_command).empty());
    assert(std::filesystem::file_size(inferred_aggregate_reassignment_object_path) > 0);

    auto branch_aggregate_reassignment_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_aggregate_reassignment.o";
    auto branch_aggregate_reassignment_object_command =
        executable.string() + " --emit-object " + branch_aggregate_reassignment_emit_path.string() + " -o " +
        branch_aggregate_reassignment_object_path.string();
    assert(read_command_output(branch_aggregate_reassignment_object_command).empty());
    assert(std::filesystem::file_size(branch_aggregate_reassignment_object_path) > 0);

    auto switch_aggregate_reassignment_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_switch_aggregate_reassignment.o";
    auto switch_aggregate_reassignment_object_command =
        executable.string() + " --emit-object " + switch_aggregate_reassignment_emit_path.string() + " -o " +
        switch_aggregate_reassignment_object_path.string();
    assert(read_command_output(switch_aggregate_reassignment_object_command).empty());
    assert(std::filesystem::file_size(switch_aggregate_reassignment_object_path) > 0);

    auto branch_aggregate_field_assignment_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_aggregate_field_assignment.o";
    auto branch_aggregate_field_assignment_object_command =
        executable.string() + " --emit-object " + branch_aggregate_field_assignment_emit_path.string() + " -o " +
        branch_aggregate_field_assignment_object_path.string();
    assert(read_command_output(branch_aggregate_field_assignment_object_command).empty());
    assert(std::filesystem::file_size(branch_aggregate_field_assignment_object_path) > 0);

    auto switch_aggregate_field_assignment_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_switch_aggregate_field_assignment.o";
    auto switch_aggregate_field_assignment_object_command =
        executable.string() + " --emit-object " + switch_aggregate_field_assignment_emit_path.string() + " -o " +
        switch_aggregate_field_assignment_object_path.string();
    assert(read_command_output(switch_aggregate_field_assignment_object_command).empty());
    assert(std::filesystem::file_size(switch_aggregate_field_assignment_object_path) > 0);

    auto branch_nested_array_assignment_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_nested_array_assignment.o";
    auto branch_nested_array_assignment_object_command =
        executable.string() + " --emit-object " + branch_nested_array_assignment_emit_path.string() + " -o " +
        branch_nested_array_assignment_object_path.string();
    assert(read_command_output(branch_nested_array_assignment_object_command).empty());
    assert(std::filesystem::file_size(branch_nested_array_assignment_object_path) > 0);

    auto switch_nested_array_assignment_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_switch_nested_array_assignment.o";
    auto switch_nested_array_assignment_object_command =
        executable.string() + " --emit-object " + switch_nested_array_assignment_emit_path.string() + " -o " +
        switch_nested_array_assignment_object_path.string();
    assert(read_command_output(switch_nested_array_assignment_object_command).empty());
    assert(std::filesystem::file_size(switch_nested_array_assignment_object_path) > 0);

    auto helper_aggregate_access_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_helper_aggregate_access.o";
    auto helper_aggregate_access_object_command =
        executable.string() + " --emit-object " + helper_aggregate_access_emit_path.string() + " -o " +
        helper_aggregate_access_object_path.string();
    assert(read_command_output(helper_aggregate_access_object_command).empty());
    assert(std::filesystem::file_size(helper_aggregate_access_object_path) > 0);

    auto aggregate_parameter_access_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_aggregate_parameter_access.o";
    auto aggregate_parameter_access_object_command =
        executable.string() + " --emit-object " + aggregate_parameter_access_emit_path.string() + " -o " +
        aggregate_parameter_access_object_path.string();
    assert(read_command_output(aggregate_parameter_access_object_command).empty());
    assert(std::filesystem::file_size(aggregate_parameter_access_object_path) > 0);

    auto call_argument_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_call_argument_aggregate_scalar.o";
    auto call_argument_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + call_argument_aggregate_scalar_emit_path.string() + " -o " +
        call_argument_aggregate_scalar_object_path.string();
    assert(read_command_output(call_argument_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(call_argument_aggregate_scalar_object_path) > 0);

    auto ffi_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_ffi_aggregate_scalar.o";
    auto ffi_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + ffi_aggregate_scalar_emit_path.string() + " -o " +
        ffi_aggregate_scalar_object_path.string();
    assert(read_command_output(ffi_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(ffi_aggregate_scalar_object_path) > 0);

    auto return_container_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_return_container_aggregate_scalar.o";
    auto return_container_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + return_container_aggregate_scalar_emit_path.string() + " -o " +
        return_container_aggregate_scalar_object_path.string();
    assert(read_command_output(return_container_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(return_container_aggregate_scalar_object_path) > 0);

    auto nested_return_container_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_nested_return_container_aggregate_scalar.o";
    auto nested_return_container_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + nested_return_container_aggregate_scalar_emit_path.string() + " -o " +
        nested_return_container_aggregate_scalar_object_path.string();
    assert(read_command_output(nested_return_container_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(nested_return_container_aggregate_scalar_object_path) > 0);

    auto branch_return_container_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_return_container_aggregate_scalar.o";
    auto branch_return_container_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + branch_return_container_aggregate_scalar_emit_path.string() + " -o " +
        branch_return_container_aggregate_scalar_object_path.string();
    assert(read_command_output(branch_return_container_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(branch_return_container_aggregate_scalar_object_path) > 0);

    auto loop_return_container_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_loop_return_container_aggregate_scalar.o";
    auto loop_return_container_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + loop_return_container_aggregate_scalar_emit_path.string() + " -o " +
        loop_return_container_aggregate_scalar_object_path.string();
    assert(read_command_output(loop_return_container_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(loop_return_container_aggregate_scalar_object_path) > 0);

    auto control_flow_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_control_flow_aggregate_scalar.o";
    auto control_flow_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + control_flow_aggregate_scalar_emit_path.string() + " -o " +
        control_flow_aggregate_scalar_object_path.string();
    assert(read_command_output(control_flow_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(control_flow_aggregate_scalar_object_path) > 0);

    auto loop_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_loop_aggregate_scalar.o";
    auto loop_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + loop_aggregate_scalar_emit_path.string() + " -o " +
        loop_aggregate_scalar_object_path.string();
    assert(read_command_output(loop_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(loop_aggregate_scalar_object_path) > 0);

    auto guard_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_guard_aggregate_scalar.o";
    auto guard_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + guard_aggregate_scalar_emit_path.string() + " -o " +
        guard_aggregate_scalar_object_path.string();
    assert(read_command_output(guard_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(guard_aggregate_scalar_object_path) > 0);

    auto defer_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_defer_aggregate_scalar.o";
    auto defer_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + defer_aggregate_scalar_emit_path.string() + " -o " +
        defer_aggregate_scalar_object_path.string();
    assert(read_command_output(defer_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(defer_aggregate_scalar_object_path) > 0);

    auto unsafe_aggregate_scalar_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_unsafe_aggregate_scalar.o";
    auto unsafe_aggregate_scalar_object_command =
        executable.string() + " --emit-object " + unsafe_aggregate_scalar_emit_path.string() + " -o " +
        unsafe_aggregate_scalar_object_path.string();
    assert(read_command_output(unsafe_aggregate_scalar_object_command).empty());
    assert(std::filesystem::file_size(unsafe_aggregate_scalar_object_path) > 0);

    auto method_aggregate_access_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_method_aggregate_access.o";
    auto method_aggregate_access_object_command =
        executable.string() + " --emit-object " + method_aggregate_access_emit_path.string() + " -o " +
        method_aggregate_access_object_path.string();
    assert(read_command_output(method_aggregate_access_object_command).empty());
    assert(std::filesystem::file_size(method_aggregate_access_object_path) > 0);

    auto record_method_aggregate_access_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_record_method_aggregate_access.o";
    auto record_method_aggregate_access_object_command =
        executable.string() + " --emit-object " + record_method_aggregate_access_emit_path.string() + " -o " +
        record_method_aggregate_access_object_path.string();
    assert(read_command_output(record_method_aggregate_access_object_command).empty());
    assert(std::filesystem::file_size(record_method_aggregate_access_object_path) > 0);

    auto member_receiver_method_aggregate_access_object_path =
        std::filesystem::temp_directory_path() / "orison_cli_member_receiver_method_aggregate_access.o";
    auto member_receiver_method_aggregate_access_object_command =
        executable.string() + " --emit-object " + member_receiver_method_aggregate_access_emit_path.string() + " -o " +
        member_receiver_method_aggregate_access_object_path.string();
    assert(read_command_output(member_receiver_method_aggregate_access_object_command).empty());
    assert(std::filesystem::file_size(member_receiver_method_aggregate_access_object_path) > 0);

    auto demo_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "minimal.or";
    auto executable_path = std::filesystem::temp_directory_path() / "orison_cli_build";
    auto build_command =
        executable.string() + " --build " + demo_path.string() + " -o " + executable_path.string();
    assert(read_command_output(build_command).empty());
    auto executable_status = std::system(executable_path.string().c_str());
    assert(WIFEXITED(executable_status));
    assert(WEXITSTATUS(executable_status) == 0);

    auto local_record_assignment_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_local_record_field_assignment_build";
    auto local_record_assignment_build_command =
        executable.string() + " --build " + local_record_assignment_emit_path.string() + " -o " +
        local_record_assignment_executable_path.string();
    assert(read_command_output(local_record_assignment_build_command).empty());
    auto local_record_assignment_executable_status =
        std::system(local_record_assignment_executable_path.string().c_str());
    assert(WIFEXITED(local_record_assignment_executable_status));
    assert(WEXITSTATUS(local_record_assignment_executable_status) == 0);

    auto pointer_record_assignment_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_pointer_record_field_assignment_build";
    auto pointer_record_assignment_build_command =
        executable.string() + " --build " + pointer_record_assignment_emit_path.string() + " -o " +
        pointer_record_assignment_executable_path.string();
    assert(read_command_output(pointer_record_assignment_build_command).empty());
    auto pointer_record_assignment_executable_status =
        std::system(pointer_record_assignment_executable_path.string().c_str());
    assert(WIFEXITED(pointer_record_assignment_executable_status));
    assert(WEXITSTATUS(pointer_record_assignment_executable_status) == 0);

    auto inferred_record_array_let_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_inferred_record_array_let_build";
    auto inferred_record_array_let_build_command =
        executable.string() + " --build " + inferred_record_array_let_emit_path.string() + " -o " +
        inferred_record_array_let_executable_path.string();
    assert(read_command_output(inferred_record_array_let_build_command).empty());
    auto inferred_record_array_let_executable_status =
        std::system(inferred_record_array_let_executable_path.string().c_str());
    assert(WIFEXITED(inferred_record_array_let_executable_status));
    assert(WEXITSTATUS(inferred_record_array_let_executable_status) == 0);

    auto inferred_array_record_let_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_inferred_array_record_let_build";
    auto inferred_array_record_let_build_command =
        executable.string() + " --build " + inferred_array_record_let_emit_path.string() + " -o " +
        inferred_array_record_let_executable_path.string();
    assert(read_command_output(inferred_array_record_let_build_command).empty());
    auto inferred_array_record_let_executable_status =
        std::system(inferred_array_record_let_executable_path.string().c_str());
    assert(WIFEXITED(inferred_array_record_let_executable_status));
    assert(WEXITSTATUS(inferred_array_record_let_executable_status) == 0);

    auto inferred_nested_mixed_let_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_inferred_nested_mixed_let_build";
    auto inferred_nested_mixed_let_build_command =
        executable.string() + " --build " + inferred_nested_mixed_let_emit_path.string() + " -o " +
        inferred_nested_mixed_let_executable_path.string();
    assert(read_command_output(inferred_nested_mixed_let_build_command).empty());
    auto inferred_nested_mixed_let_executable_status =
        std::system(inferred_nested_mixed_let_executable_path.string().c_str());
    assert(WIFEXITED(inferred_nested_mixed_let_executable_status));
    assert(WEXITSTATUS(inferred_nested_mixed_let_executable_status) == 0);

    auto branch_inferred_aggregate_let_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_inferred_aggregate_let_build";
    auto branch_inferred_aggregate_let_build_command =
        executable.string() + " --build " + branch_inferred_aggregate_let_emit_path.string() + " -o " +
        branch_inferred_aggregate_let_executable_path.string();
    assert(read_command_output(branch_inferred_aggregate_let_build_command).empty());
    auto branch_inferred_aggregate_let_executable_status =
        std::system(branch_inferred_aggregate_let_executable_path.string().c_str());
    assert(WIFEXITED(branch_inferred_aggregate_let_executable_status));
    assert(WEXITSTATUS(branch_inferred_aggregate_let_executable_status) == 0);

    auto inferred_aggregate_reassignment_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_inferred_aggregate_reassignment_build";
    auto inferred_aggregate_reassignment_build_command =
        executable.string() + " --build " + inferred_aggregate_reassignment_emit_path.string() + " -o " +
        inferred_aggregate_reassignment_executable_path.string();
    assert(read_command_output(inferred_aggregate_reassignment_build_command).empty());
    auto inferred_aggregate_reassignment_executable_status =
        std::system(inferred_aggregate_reassignment_executable_path.string().c_str());
    assert(WIFEXITED(inferred_aggregate_reassignment_executable_status));
    assert(WEXITSTATUS(inferred_aggregate_reassignment_executable_status) == 0);

    auto branch_aggregate_reassignment_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_aggregate_reassignment_build";
    auto branch_aggregate_reassignment_build_command =
        executable.string() + " --build " + branch_aggregate_reassignment_emit_path.string() + " -o " +
        branch_aggregate_reassignment_executable_path.string();
    assert(read_command_output(branch_aggregate_reassignment_build_command).empty());
    auto branch_aggregate_reassignment_executable_status =
        std::system(branch_aggregate_reassignment_executable_path.string().c_str());
    assert(WIFEXITED(branch_aggregate_reassignment_executable_status));
    assert(WEXITSTATUS(branch_aggregate_reassignment_executable_status) == 0);

    auto switch_aggregate_reassignment_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_switch_aggregate_reassignment_build";
    auto switch_aggregate_reassignment_build_command =
        executable.string() + " --build " + switch_aggregate_reassignment_emit_path.string() + " -o " +
        switch_aggregate_reassignment_executable_path.string();
    assert(read_command_output(switch_aggregate_reassignment_build_command).empty());
    auto switch_aggregate_reassignment_executable_status =
        std::system(switch_aggregate_reassignment_executable_path.string().c_str());
    assert(WIFEXITED(switch_aggregate_reassignment_executable_status));
    assert(WEXITSTATUS(switch_aggregate_reassignment_executable_status) == 0);

    auto branch_aggregate_field_assignment_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_aggregate_field_assignment_build";
    auto branch_aggregate_field_assignment_build_command =
        executable.string() + " --build " + branch_aggregate_field_assignment_emit_path.string() + " -o " +
        branch_aggregate_field_assignment_executable_path.string();
    assert(read_command_output(branch_aggregate_field_assignment_build_command).empty());
    auto branch_aggregate_field_assignment_executable_status =
        std::system(branch_aggregate_field_assignment_executable_path.string().c_str());
    assert(WIFEXITED(branch_aggregate_field_assignment_executable_status));
    assert(WEXITSTATUS(branch_aggregate_field_assignment_executable_status) == 0);

    auto switch_aggregate_field_assignment_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_switch_aggregate_field_assignment_build";
    auto switch_aggregate_field_assignment_build_command =
        executable.string() + " --build " + switch_aggregate_field_assignment_emit_path.string() + " -o " +
        switch_aggregate_field_assignment_executable_path.string();
    assert(read_command_output(switch_aggregate_field_assignment_build_command).empty());
    auto switch_aggregate_field_assignment_executable_status =
        std::system(switch_aggregate_field_assignment_executable_path.string().c_str());
    assert(WIFEXITED(switch_aggregate_field_assignment_executable_status));
    assert(WEXITSTATUS(switch_aggregate_field_assignment_executable_status) == 0);

    auto branch_nested_array_assignment_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_nested_array_assignment_build";
    auto branch_nested_array_assignment_build_command =
        executable.string() + " --build " + branch_nested_array_assignment_emit_path.string() + " -o " +
        branch_nested_array_assignment_executable_path.string();
    assert(read_command_output(branch_nested_array_assignment_build_command).empty());
    auto branch_nested_array_assignment_executable_status =
        std::system(branch_nested_array_assignment_executable_path.string().c_str());
    assert(WIFEXITED(branch_nested_array_assignment_executable_status));
    assert(WEXITSTATUS(branch_nested_array_assignment_executable_status) == 0);

    auto switch_nested_array_assignment_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_switch_nested_array_assignment_build";
    auto switch_nested_array_assignment_build_command =
        executable.string() + " --build " + switch_nested_array_assignment_emit_path.string() + " -o " +
        switch_nested_array_assignment_executable_path.string();
    assert(read_command_output(switch_nested_array_assignment_build_command).empty());
    auto switch_nested_array_assignment_executable_status =
        std::system(switch_nested_array_assignment_executable_path.string().c_str());
    assert(WIFEXITED(switch_nested_array_assignment_executable_status));
    assert(WEXITSTATUS(switch_nested_array_assignment_executable_status) == 0);

    auto helper_aggregate_access_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_helper_aggregate_access_build";
    auto helper_aggregate_access_build_command =
        executable.string() + " --build " + helper_aggregate_access_emit_path.string() + " -o " +
        helper_aggregate_access_executable_path.string();
    assert(read_command_output(helper_aggregate_access_build_command).empty());
    auto helper_aggregate_access_executable_status =
        std::system(helper_aggregate_access_executable_path.string().c_str());
    assert(WIFEXITED(helper_aggregate_access_executable_status));
    assert(WEXITSTATUS(helper_aggregate_access_executable_status) == 0);

    auto aggregate_parameter_access_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_aggregate_parameter_access_build";
    auto aggregate_parameter_access_build_command =
        executable.string() + " --build " + aggregate_parameter_access_emit_path.string() + " -o " +
        aggregate_parameter_access_executable_path.string();
    assert(read_command_output(aggregate_parameter_access_build_command).empty());
    auto aggregate_parameter_access_executable_status =
        std::system(aggregate_parameter_access_executable_path.string().c_str());
    assert(WIFEXITED(aggregate_parameter_access_executable_status));
    assert(WEXITSTATUS(aggregate_parameter_access_executable_status) == 0);

    auto call_argument_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_call_argument_aggregate_scalar_build";
    auto call_argument_aggregate_scalar_build_command =
        executable.string() + " --build " + call_argument_aggregate_scalar_emit_path.string() + " -o " +
        call_argument_aggregate_scalar_executable_path.string();
    assert(read_command_output(call_argument_aggregate_scalar_build_command).empty());
    auto call_argument_aggregate_scalar_executable_status =
        std::system(call_argument_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(call_argument_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(call_argument_aggregate_scalar_executable_status) == 0);

    auto ffi_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_ffi_aggregate_scalar_build";
    auto ffi_aggregate_scalar_build_command =
        executable.string() + " --build " + ffi_aggregate_scalar_emit_path.string() + " -o " +
        ffi_aggregate_scalar_executable_path.string();
    assert(read_command_output(ffi_aggregate_scalar_build_command).empty());
    auto ffi_aggregate_scalar_executable_status =
        std::system((ffi_aggregate_scalar_executable_path.string() + " >/dev/null").c_str());
    assert(WIFEXITED(ffi_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(ffi_aggregate_scalar_executable_status) == 0);

    auto return_container_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_return_container_aggregate_scalar_build";
    auto return_container_aggregate_scalar_build_command =
        executable.string() + " --build " + return_container_aggregate_scalar_emit_path.string() + " -o " +
        return_container_aggregate_scalar_executable_path.string();
    assert(read_command_output(return_container_aggregate_scalar_build_command).empty());
    auto return_container_aggregate_scalar_executable_status =
        std::system(return_container_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(return_container_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(return_container_aggregate_scalar_executable_status) == 0);

    auto nested_return_container_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_nested_return_container_aggregate_scalar_build";
    auto nested_return_container_aggregate_scalar_build_command =
        executable.string() + " --build " + nested_return_container_aggregate_scalar_emit_path.string() + " -o " +
        nested_return_container_aggregate_scalar_executable_path.string();
    assert(read_command_output(nested_return_container_aggregate_scalar_build_command).empty());
    auto nested_return_container_aggregate_scalar_executable_status =
        std::system(nested_return_container_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(nested_return_container_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(nested_return_container_aggregate_scalar_executable_status) == 0);

    auto branch_return_container_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_branch_return_container_aggregate_scalar_build";
    auto branch_return_container_aggregate_scalar_build_command =
        executable.string() + " --build " + branch_return_container_aggregate_scalar_emit_path.string() + " -o " +
        branch_return_container_aggregate_scalar_executable_path.string();
    assert(read_command_output(branch_return_container_aggregate_scalar_build_command).empty());
    auto branch_return_container_aggregate_scalar_executable_status =
        std::system(branch_return_container_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(branch_return_container_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(branch_return_container_aggregate_scalar_executable_status) == 0);

    auto loop_return_container_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_loop_return_container_aggregate_scalar_build";
    auto loop_return_container_aggregate_scalar_build_command =
        executable.string() + " --build " + loop_return_container_aggregate_scalar_emit_path.string() + " -o " +
        loop_return_container_aggregate_scalar_executable_path.string();
    assert(read_command_output(loop_return_container_aggregate_scalar_build_command).empty());
    auto loop_return_container_aggregate_scalar_executable_status =
        std::system(loop_return_container_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(loop_return_container_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(loop_return_container_aggregate_scalar_executable_status) == 0);

    auto control_flow_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_control_flow_aggregate_scalar_build";
    auto control_flow_aggregate_scalar_build_command =
        executable.string() + " --build " + control_flow_aggregate_scalar_emit_path.string() + " -o " +
        control_flow_aggregate_scalar_executable_path.string();
    assert(read_command_output(control_flow_aggregate_scalar_build_command).empty());
    auto control_flow_aggregate_scalar_executable_status =
        std::system(control_flow_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(control_flow_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(control_flow_aggregate_scalar_executable_status) == 0);

    auto loop_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_loop_aggregate_scalar_build";
    auto loop_aggregate_scalar_build_command =
        executable.string() + " --build " + loop_aggregate_scalar_emit_path.string() + " -o " +
        loop_aggregate_scalar_executable_path.string();
    assert(read_command_output(loop_aggregate_scalar_build_command).empty());
    auto loop_aggregate_scalar_executable_status =
        std::system(loop_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(loop_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(loop_aggregate_scalar_executable_status) == 0);

    auto guard_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_guard_aggregate_scalar_build";
    auto guard_aggregate_scalar_build_command =
        executable.string() + " --build " + guard_aggregate_scalar_emit_path.string() + " -o " +
        guard_aggregate_scalar_executable_path.string();
    assert(read_command_output(guard_aggregate_scalar_build_command).empty());
    auto guard_aggregate_scalar_executable_status =
        std::system(guard_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(guard_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(guard_aggregate_scalar_executable_status) == 0);

    auto defer_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_defer_aggregate_scalar_build";
    auto defer_aggregate_scalar_build_command =
        executable.string() + " --build " + defer_aggregate_scalar_emit_path.string() + " -o " +
        defer_aggregate_scalar_executable_path.string();
    assert(read_command_output(defer_aggregate_scalar_build_command).empty());
    auto defer_aggregate_scalar_executable_status =
        std::system(defer_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(defer_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(defer_aggregate_scalar_executable_status) == 0);

    auto unsafe_aggregate_scalar_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_unsafe_aggregate_scalar_build";
    auto unsafe_aggregate_scalar_build_command =
        executable.string() + " --build " + unsafe_aggregate_scalar_emit_path.string() + " -o " +
        unsafe_aggregate_scalar_executable_path.string();
    assert(read_command_output(unsafe_aggregate_scalar_build_command).empty());
    auto unsafe_aggregate_scalar_executable_status =
        std::system(unsafe_aggregate_scalar_executable_path.string().c_str());
    assert(WIFEXITED(unsafe_aggregate_scalar_executable_status));
    assert(WEXITSTATUS(unsafe_aggregate_scalar_executable_status) == 0);

    auto method_aggregate_access_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_method_aggregate_access_build";
    auto method_aggregate_access_build_command =
        executable.string() + " --build " + method_aggregate_access_emit_path.string() + " -o " +
        method_aggregate_access_executable_path.string();
    assert(read_command_output(method_aggregate_access_build_command).empty());
    auto method_aggregate_access_executable_status =
        std::system(method_aggregate_access_executable_path.string().c_str());
    assert(WIFEXITED(method_aggregate_access_executable_status));
    assert(WEXITSTATUS(method_aggregate_access_executable_status) == 0);

    auto record_method_aggregate_access_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_record_method_aggregate_access_build";
    auto record_method_aggregate_access_build_command =
        executable.string() + " --build " + record_method_aggregate_access_emit_path.string() + " -o " +
        record_method_aggregate_access_executable_path.string();
    assert(read_command_output(record_method_aggregate_access_build_command).empty());
    auto record_method_aggregate_access_executable_status =
        std::system(record_method_aggregate_access_executable_path.string().c_str());
    assert(WIFEXITED(record_method_aggregate_access_executable_status));
    assert(WEXITSTATUS(record_method_aggregate_access_executable_status) == 0);

    auto member_receiver_method_aggregate_access_executable_path =
        std::filesystem::temp_directory_path() / "orison_cli_member_receiver_method_aggregate_access_build";
    auto member_receiver_method_aggregate_access_build_command =
        executable.string() + " --build " + member_receiver_method_aggregate_access_emit_path.string() + " -o " +
        member_receiver_method_aggregate_access_executable_path.string();
    assert(read_command_output(member_receiver_method_aggregate_access_build_command).empty());
    auto member_receiver_method_aggregate_access_executable_status =
        std::system(member_receiver_method_aggregate_access_executable_path.string().c_str());
    assert(WIFEXITED(member_receiver_method_aggregate_access_executable_status));
    assert(WEXITSTATUS(member_receiver_method_aggregate_access_executable_status) == 0);

    auto run_command = executable.string() + " run " + demo_path.string();
    auto run_status = std::system(run_command.c_str());
    assert(WIFEXITED(run_status));
    assert(WEXITSTATUS(run_status) == 0);

    auto ffi_demo_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "tour_09_ffi_printf.or";
    auto ffi_output = read_command_output_with_exit_code(
        executable.string() + " run " + ffi_demo_path.string(),
        25
    );
    assert(ffi_output == "Hello world from Orison!\n");

    auto fixed_ffi_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "ffi_fixed_parameters.or";
    auto fixed_ffi_status = std::system((executable.string() + " run " + fixed_ffi_path.string()).c_str());
    assert(WIFEXITED(fixed_ffi_status));
    assert(WEXITSTATUS(fixed_ffi_status) == 0);

    auto ffi_aggregate_scalar_status = std::system(
        (executable.string() + " run " + ffi_aggregate_scalar_emit_path.string() + " >/dev/null").c_str()
    );
    assert(WIFEXITED(ffi_aggregate_scalar_status));
    assert(WEXITSTATUS(ffi_aggregate_scalar_status) == 0);

    auto immutable_aggregate_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_aggregate_let.or";
    auto immutable_aggregate_status =
        std::system((executable.string() + " run " + immutable_aggregate_demo_path.string()).c_str());
    assert(WIFEXITED(immutable_aggregate_status));
    assert(WEXITSTATUS(immutable_aggregate_status) == 0);

    auto inferred_record_let_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_record_let.or";
    auto inferred_record_let_status =
        std::system((executable.string() + " run " + inferred_record_let_demo_path.string()).c_str());
    assert(WIFEXITED(inferred_record_let_status));
    assert(WEXITSTATUS(inferred_record_let_status) == 0);

    auto inferred_nested_record_let_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_nested_record_let.or";
    auto inferred_nested_record_let_status =
        std::system((executable.string() + " run " + inferred_nested_record_let_demo_path.string()).c_str());
    assert(WIFEXITED(inferred_nested_record_let_status));
    assert(WEXITSTATUS(inferred_nested_record_let_status) == 0);

    auto inferred_record_array_let_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_record_array_let.or";
    auto inferred_record_array_let_status =
        std::system((executable.string() + " run " + inferred_record_array_let_demo_path.string()).c_str());
    assert(WIFEXITED(inferred_record_array_let_status));
    assert(WEXITSTATUS(inferred_record_array_let_status) == 0);

    auto inferred_array_let_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_array_let.or";
    auto inferred_array_let_status =
        std::system((executable.string() + " run " + inferred_array_let_demo_path.string()).c_str());
    assert(WIFEXITED(inferred_array_let_status));
    assert(WEXITSTATUS(inferred_array_let_status) == 0);

    auto inferred_nested_array_let_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_nested_array_let.or";
    auto inferred_nested_array_let_status =
        std::system((executable.string() + " run " + inferred_nested_array_let_demo_path.string()).c_str());
    assert(WIFEXITED(inferred_nested_array_let_status));
    assert(WEXITSTATUS(inferred_nested_array_let_status) == 0);

    auto inferred_array_record_let_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_array_record_let.or";
    auto inferred_array_record_let_status =
        std::system((executable.string() + " run " + inferred_array_record_let_demo_path.string()).c_str());
    assert(WIFEXITED(inferred_array_record_let_status));
    assert(WEXITSTATUS(inferred_array_record_let_status) == 0);

    auto inferred_nested_mixed_let_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_nested_mixed_let.or";
    auto inferred_nested_mixed_let_status =
        std::system((executable.string() + " run " + inferred_nested_mixed_let_demo_path.string()).c_str());
    assert(WIFEXITED(inferred_nested_mixed_let_status));
    assert(WEXITSTATUS(inferred_nested_mixed_let_status) == 0);

    auto branch_inferred_aggregate_let_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_inferred_aggregate_let.or";
    auto branch_inferred_aggregate_let_status =
        std::system((executable.string() + " run " + branch_inferred_aggregate_let_demo_path.string()).c_str());
    assert(WIFEXITED(branch_inferred_aggregate_let_status));
    assert(WEXITSTATUS(branch_inferred_aggregate_let_status) == 0);

    auto inferred_aggregate_reassignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_inferred_aggregate_reassignment.or";
    auto inferred_aggregate_reassignment_status = std::system(
        (executable.string() + " run " + inferred_aggregate_reassignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(inferred_aggregate_reassignment_status));
    assert(WEXITSTATUS(inferred_aggregate_reassignment_status) == 0);

    auto branch_aggregate_reassignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_aggregate_reassignment.or";
    auto branch_aggregate_reassignment_status = std::system(
        (executable.string() + " run " + branch_aggregate_reassignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(branch_aggregate_reassignment_status));
    assert(WEXITSTATUS(branch_aggregate_reassignment_status) == 0);

    auto switch_aggregate_reassignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_switch_aggregate_reassignment.or";
    auto switch_aggregate_reassignment_status = std::system(
        (executable.string() + " run " + switch_aggregate_reassignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(switch_aggregate_reassignment_status));
    assert(WEXITSTATUS(switch_aggregate_reassignment_status) == 0);

    auto branch_aggregate_field_assignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_aggregate_field_assignment.or";
    auto branch_aggregate_field_assignment_status = std::system(
        (executable.string() + " run " + branch_aggregate_field_assignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(branch_aggregate_field_assignment_status));
    assert(WEXITSTATUS(branch_aggregate_field_assignment_status) == 0);

    auto switch_aggregate_field_assignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_switch_aggregate_field_assignment.or";
    auto switch_aggregate_field_assignment_status = std::system(
        (executable.string() + " run " + switch_aggregate_field_assignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(switch_aggregate_field_assignment_status));
    assert(WEXITSTATUS(switch_aggregate_field_assignment_status) == 0);

    auto branch_nested_array_assignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_nested_array_assignment.or";
    auto branch_nested_array_assignment_status = std::system(
        (executable.string() + " run " + branch_nested_array_assignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(branch_nested_array_assignment_status));
    assert(WEXITSTATUS(branch_nested_array_assignment_status) == 0);

    auto switch_nested_array_assignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_switch_nested_array_assignment.or";
    auto switch_nested_array_assignment_status = std::system(
        (executable.string() + " run " + switch_nested_array_assignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(switch_nested_array_assignment_status));
    assert(WEXITSTATUS(switch_nested_array_assignment_status) == 0);

    auto helper_aggregate_access_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_helper_aggregate_access.or";
    auto helper_aggregate_access_status = std::system(
        (executable.string() + " run " + helper_aggregate_access_demo_path.string()).c_str()
    );
    assert(WIFEXITED(helper_aggregate_access_status));
    assert(WEXITSTATUS(helper_aggregate_access_status) == 0);

    auto aggregate_parameter_access_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_aggregate_parameter_access.or";
    auto aggregate_parameter_access_status = std::system(
        (executable.string() + " run " + aggregate_parameter_access_demo_path.string()).c_str()
    );
    assert(WIFEXITED(aggregate_parameter_access_status));
    assert(WEXITSTATUS(aggregate_parameter_access_status) == 0);

    auto call_argument_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_call_argument_aggregate_scalar.or";
    auto call_argument_aggregate_scalar_status = std::system(
        (executable.string() + " run " + call_argument_aggregate_scalar_demo_path.string()).c_str()
    );
    assert(WIFEXITED(call_argument_aggregate_scalar_status));
    assert(WEXITSTATUS(call_argument_aggregate_scalar_status) == 0);

    auto return_container_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_return_container_aggregate_scalar.or";
    auto return_container_aggregate_scalar_status = std::system(
        (executable.string() + " run " + return_container_aggregate_scalar_demo_path.string()).c_str()
    );
    assert(WIFEXITED(return_container_aggregate_scalar_status));
    assert(WEXITSTATUS(return_container_aggregate_scalar_status) == 0);

    auto nested_return_container_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_nested_return_container_aggregate_scalar.or";
    auto nested_return_container_aggregate_scalar_status = std::system(
        (executable.string() + " run " + nested_return_container_aggregate_scalar_demo_path.string()).c_str()
    );
    assert(WIFEXITED(nested_return_container_aggregate_scalar_status));
    assert(WEXITSTATUS(nested_return_container_aggregate_scalar_status) == 0);

    auto branch_return_container_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_branch_return_container_aggregate_scalar.or";
    auto branch_return_container_aggregate_scalar_status = std::system(
        (executable.string() + " run " + branch_return_container_aggregate_scalar_demo_path.string()).c_str()
    );
    assert(WIFEXITED(branch_return_container_aggregate_scalar_status));
    assert(WEXITSTATUS(branch_return_container_aggregate_scalar_status) == 0);

    auto loop_return_container_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_loop_return_container_aggregate_scalar.or";
    auto loop_return_container_aggregate_scalar_status = std::system(
        (executable.string() + " run " + loop_return_container_aggregate_scalar_demo_path.string()).c_str()
    );
    assert(WIFEXITED(loop_return_container_aggregate_scalar_status));
    assert(WEXITSTATUS(loop_return_container_aggregate_scalar_status) == 0);

    auto control_flow_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_control_flow_aggregate_scalar.or";
    auto control_flow_aggregate_scalar_status = std::system(
        (executable.string() + " run " + control_flow_aggregate_scalar_demo_path.string()).c_str()
    );
    assert(WIFEXITED(control_flow_aggregate_scalar_status));
    assert(WEXITSTATUS(control_flow_aggregate_scalar_status) == 0);

    auto loop_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_loop_aggregate_scalar.or";
    auto loop_aggregate_scalar_status =
        std::system((executable.string() + " run " + loop_aggregate_scalar_demo_path.string()).c_str());
    assert(WIFEXITED(loop_aggregate_scalar_status));
    assert(WEXITSTATUS(loop_aggregate_scalar_status) == 0);

    auto guard_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_guard_aggregate_scalar.or";
    auto guard_aggregate_scalar_status =
        std::system((executable.string() + " run " + guard_aggregate_scalar_demo_path.string()).c_str());
    assert(WIFEXITED(guard_aggregate_scalar_status));
    assert(WEXITSTATUS(guard_aggregate_scalar_status) == 0);

    auto defer_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_defer_aggregate_scalar.or";
    auto defer_aggregate_scalar_status =
        std::system((executable.string() + " run " + defer_aggregate_scalar_demo_path.string()).c_str());
    assert(WIFEXITED(defer_aggregate_scalar_status));
    assert(WEXITSTATUS(defer_aggregate_scalar_status) == 0);

    auto unsafe_aggregate_scalar_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_unsafe_aggregate_scalar.or";
    auto unsafe_aggregate_scalar_status =
        std::system((executable.string() + " run " + unsafe_aggregate_scalar_demo_path.string()).c_str());
    assert(WIFEXITED(unsafe_aggregate_scalar_status));
    assert(WEXITSTATUS(unsafe_aggregate_scalar_status) == 0);

    auto method_aggregate_access_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_method_aggregate_access.or";
    auto method_aggregate_access_status = std::system(
        (executable.string() + " run " + method_aggregate_access_demo_path.string()).c_str()
    );
    assert(WIFEXITED(method_aggregate_access_status));
    assert(WEXITSTATUS(method_aggregate_access_status) == 0);

    auto record_method_aggregate_access_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_method_aggregate_access.or";
    auto record_method_aggregate_access_status = std::system(
        (executable.string() + " run " + record_method_aggregate_access_demo_path.string()).c_str()
    );
    assert(WIFEXITED(record_method_aggregate_access_status));
    assert(WEXITSTATUS(record_method_aggregate_access_status) == 0);

    auto member_receiver_method_aggregate_access_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" /
        "local_member_receiver_method_aggregate_access.or";
    auto member_receiver_method_aggregate_access_status = std::system(
        (executable.string() + " run " + member_receiver_method_aggregate_access_demo_path.string()).c_str()
    );
    assert(WIFEXITED(member_receiver_method_aggregate_access_status));
    assert(WEXITSTATUS(member_receiver_method_aggregate_access_status) == 0);

    auto array_for_demo_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_array_for.or";
    auto array_for_status = std::system((executable.string() + " run " + array_for_demo_path.string()).c_str());
    assert(WIFEXITED(array_for_status));
    assert(WEXITSTATUS(array_for_status) == 0);

    auto ternary_array_for_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_ternary_array_for.or";
    auto ternary_array_for_status =
        std::system((executable.string() + " run " + ternary_array_for_demo_path.string()).c_str());
    assert(WIFEXITED(ternary_array_for_status));
    assert(WEXITSTATUS(ternary_array_for_status) == 0);

    auto ternary_array_literal_for_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_ternary_array_literal_for.or";
    auto ternary_array_literal_for_status =
        std::system((executable.string() + " run " + ternary_array_literal_for_demo_path.string()).c_str());
    assert(WIFEXITED(ternary_array_literal_for_status));
    assert(WEXITSTATUS(ternary_array_literal_for_status) == 0);

    auto ternary_record_array_literal_for_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_ternary_record_array_literal_for.or";
    auto ternary_record_array_literal_for_status =
        std::system((executable.string() + " run " + ternary_record_array_literal_for_demo_path.string()).c_str());
    assert(WIFEXITED(ternary_record_array_literal_for_status));
    assert(WEXITSTATUS(ternary_record_array_literal_for_status) == 0);

    auto nested_array_for_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_array_for.or";
    auto nested_array_for_status = std::system(
        (executable.string() + " run " + nested_array_for_demo_path.string()).c_str()
    );
    assert(WIFEXITED(nested_array_for_status));
    assert(WEXITSTATUS(nested_array_for_status) == 0);

    auto indexed_array_for_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_index_for.or";
    auto indexed_array_for_status = std::system(
        (executable.string() + " run " + indexed_array_for_demo_path.string()).c_str()
    );
    assert(WIFEXITED(indexed_array_for_status));
    assert(WEXITSTATUS(indexed_array_for_status) == 0);

    auto indexed_field_for_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_index_field_for.or";
    auto indexed_field_for_status = std::system(
        (executable.string() + " run " + indexed_field_for_demo_path.string()).c_str()
    );
    assert(WIFEXITED(indexed_field_for_status));
    assert(WEXITSTATUS(indexed_field_for_status) == 0);

    auto helper_array_for_demo_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" /
        "local_helper_array_for.or";
    auto helper_array_for_status = std::system(
        (executable.string() + " run " + helper_array_for_demo_path.string()).c_str()
    );
    assert(WIFEXITED(helper_array_for_status));
    assert(WEXITSTATUS(helper_array_for_status) == 0);

    auto method_array_for_demo_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" /
        "local_method_array_for.or";
    auto method_array_for_status = std::system(
        (executable.string() + " run " + method_array_for_demo_path.string()).c_str()
    );
    assert(WIFEXITED(method_array_for_status));
    assert(WEXITSTATUS(method_array_for_status) == 0);

    auto member_receiver_method_array_for_demo_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" /
        "local_member_receiver_method_array_for.or";
    auto member_receiver_method_array_for_status = std::system(
        (executable.string() + " run " + member_receiver_method_array_for_demo_path.string()).c_str()
    );
    assert(WIFEXITED(member_receiver_method_array_for_status));
    assert(WEXITSTATUS(member_receiver_method_array_for_status) == 0);

    auto record_method_array_for_demo_path = std::filesystem::path(ORISON_SOURCE_DIR) / "examples" /
        "local_record_method_array_for.or";
    auto record_method_array_for_status = std::system(
        (executable.string() + " run " + record_method_array_for_demo_path.string()).c_str()
    );
    assert(WIFEXITED(record_method_array_for_status));
    assert(WEXITSTATUS(record_method_array_for_status) == 0);

    auto nested_immutable_aggregate_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_nested_aggregate_let.or";
    auto nested_immutable_aggregate_status = std::system(
        (executable.string() + " run " + nested_immutable_aggregate_demo_path.string()).c_str()
    );
    assert(WIFEXITED(nested_immutable_aggregate_status));
    assert(WEXITSTATUS(nested_immutable_aggregate_status) == 0);

    auto nested_aggregate_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "nested_pointer_aggregate_assignment.or";
    auto nested_aggregate_status =
        std::system((executable.string() + " run " + nested_aggregate_demo_path.string()).c_str());
    assert(WIFEXITED(nested_aggregate_status));
    assert(WEXITSTATUS(nested_aggregate_status) == 0);

    auto pointer_array_assignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "pointer_array_nested_assignment.or";
    auto pointer_array_assignment_status = std::system(
        (executable.string() + " run " + pointer_array_assignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(pointer_array_assignment_status));
    assert(WEXITSTATUS(pointer_array_assignment_status) == 0);

    auto pointer_record_assignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "pointer_record_field_assignment.or";
    auto pointer_record_assignment_status = std::system(
        (executable.string() + " run " + pointer_record_assignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(pointer_record_assignment_status));
    assert(WEXITSTATUS(pointer_record_assignment_status) == 0);

    auto pointer_nested_addressing_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "pointer_record_nested_addressing.or";
    auto pointer_nested_addressing_status = std::system(
        (executable.string() + " run " + pointer_nested_addressing_demo_path.string()).c_str()
    );
    assert(WIFEXITED(pointer_nested_addressing_status));
    assert(WEXITSTATUS(pointer_nested_addressing_status) == 0);

    auto local_aggregate_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_aggregate_reassignment.or";
    auto local_aggregate_status =
        std::system((executable.string() + " run " + local_aggregate_demo_path.string()).c_str());
    assert(WIFEXITED(local_aggregate_status));
    assert(WEXITSTATUS(local_aggregate_status) == 0);

    auto local_record_assignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_field_assignment.or";
    auto local_record_assignment_status = std::system(
        (executable.string() + " run " + local_record_assignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(local_record_assignment_status));
    assert(WEXITSTATUS(local_record_assignment_status) == 0);

    auto nested_addressing_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_nested_addressing.or";
    auto nested_addressing_status =
        std::system((executable.string() + " run " + nested_addressing_demo_path.string()).c_str());
    assert(WIFEXITED(nested_addressing_status));
    assert(WEXITSTATUS(nested_addressing_status) == 0);

    auto nested_record_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_nested_record_addressing.or";
    auto nested_record_status =
        std::system((executable.string() + " run " + nested_record_demo_path.string()).c_str());
    assert(WIFEXITED(nested_record_status));
    assert(WEXITSTATUS(nested_record_status) == 0);

    auto nested_record_assignment_demo_path =
        std::filesystem::path(ORISON_SOURCE_DIR) / "examples" / "local_record_nested_record_assignment.or";
    auto nested_record_assignment_status = std::system(
        (executable.string() + " run " + nested_record_assignment_demo_path.string()).c_str()
    );
    assert(WIFEXITED(nested_record_assignment_status));
    assert(WEXITSTATUS(nested_record_assignment_status) == 0);

    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unknown_choice_constant.or",
        maybe_choice_constant_lines("const DEFAULT_VALUE: Maybe<UInt32> = Missing(1)"),
        "choice constructor 'Missing' does not match any declared choice variant for constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_wrong_choice_constant.or",
        maybe_result_choice_constant_lines("const DEFAULT_VALUE: Maybe<UInt32> = Error(\"missing\")"),
        "choice constructor 'Error' does not belong to declared constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_constant_arity.or",
        status_choice_constant_lines("const DEFAULT_STATUS: Status = Ready()"),
        "choice constructor 'Ready' expects 1 payload value but received 0"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_zero_payload_choice_constant_arity.or",
        status_choice_constant_lines("const DEFAULT_STATUS: Status = Empty(1)"),
        "choice constructor 'Empty' expects 0 payload values but received 1"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_zero_payload_choice_constant_arity.or",
        boxed_maybe_choice_constant_lines("const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Empty(1))"),
        "choice constructor 'Empty' expects 0 payload values but received 1"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_wrong_choice_constant.or",
        boxed_maybe_result_choice_constant_lines(
            "const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Error(\"missing\"))"
        ),
        "choice constructor 'Error' does not belong to declared constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_unknown_choice_constant.or",
        boxed_maybe_choice_constant_lines("const DEFAULT_VALUE: Boxed<Maybe<UInt32>> = Wrap(Missing(1))"),
        "choice constructor 'Missing' does not match any declared choice variant for constant type 'Maybe<UInt32>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_constant_payload.or",
        status_choice_constant_lines("const DEFAULT_STATUS: Status = Ready(true)"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_constant_repeated_payload_type.or",
        maybe_choice_constant_lines_with_declarations(
            "const DEFAULT_VALUE: Both<Header> = Pair(Header([1, 2], 1), OtherHeader([1, 2], 1))",
            {
                "record Header",
                "    magic: Array<UInt32, 2>",
                "    version: UInt16",
                "record OtherHeader",
                "    magic: Array<UInt32, 2>",
                "    version: UInt16",
                "choice Both<T>",
                "    Pair(left: T, right: T)",
            }
        ),
        "choice constructor payload type 'OtherHeader' does not match expected payload type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_return_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    return Some(true)",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_expression_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    Some(true)",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_expression_type.or",
        ordinary_final_if_lines("true", "1 as UInt32"),
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_expression_type.or",
        ordinary_final_switch_lines("true", "1 as UInt32"),
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_expression_success.or",
        ordinary_final_if_lines("1 as UInt32", "2 as UInt32")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_expression_success.or",
        ordinary_final_switch_lines("1 as UInt32", "2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_return_type.or",
        ordinary_final_if_lines("return true", "return 1 as UInt32"),
        "return expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_return_type.or",
        ordinary_final_switch_lines("return true", "return 1 as UInt32"),
        "return expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_return_success.or",
        ordinary_final_if_lines("return 1 as UInt32", "return 2 as UInt32")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_return_success.or",
        ordinary_final_switch_lines("return 1 as UInt32", "return 2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_ternary_expression_type.or",
        ordinary_final_ternary_lines("flag ? true : 1 as UInt32"),
        "final expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_ternary_expression_success.or",
        ordinary_final_ternary_lines("flag ? 1 as UInt32 : 2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_ternary_return_type.or",
        ordinary_final_ternary_lines("return flag ? true : 1 as UInt32"),
        "return expression type 'Bool' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_ternary_return_success.or",
        ordinary_final_ternary_lines("return flag ? 1 as UInt32 : 2 as UInt32")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_final_if_without_else_value.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_final_unsafe_without_value.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_final_switch_non_value_case.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_non_unit_empty_return.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> UInt32",
            "    return",
        },
        "return statement must return a value for declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unit_empty_return.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> Unit",
            "    return",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unit_final_if_without_else.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo(flag: Bool) -> Unit",
            "    if flag",
            "        let value = 1 as UInt32",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unit_final_unsafe_without_value.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function demo() -> Unit",
            "    unsafe",
            "        let value = 1 as UInt32",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_unit_final_switch_non_value_case.or",
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
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_payload_type.or",
        choice_final_if_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_payload_type.or",
        choice_final_switch_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_return_payload_type.or",
        choice_final_if_lines("return Some(true)", "return Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_return_payload_type.or",
        choice_final_switch_lines("return Some(true)", "return Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_success.or",
        choice_final_if_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_success.or",
        choice_final_switch_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_return_success.or",
        choice_final_if_lines("return Some(1 as UInt32)", "return Empty")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_return_success.or",
        choice_final_switch_lines("return Some(1 as UInt32)", "return Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_ternary_payload_type.or",
        choice_final_ternary_lines("flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_ternary_return_payload_type.or",
        choice_final_ternary_lines("return flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_ternary_success.or",
        choice_final_ternary_lines("flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_ternary_return_success.or",
        choice_final_ternary_lines("return flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unsafe_final_ternary_payload_type.or",
        choice_final_unsafe_block_ternary_lines("flag ? Some(true) : Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unsafe_final_ternary_success.or",
        choice_final_unsafe_block_ternary_lines("flag ? Some(1 as UInt32) : Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_ternary_payload_type.or",
        choice_final_if_ternary_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_if_ternary_success.or",
        choice_final_if_ternary_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_ternary_payload_type.or",
        choice_final_switch_ternary_lines("Some(true)", "Empty"),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_final_switch_ternary_success.or",
        choice_final_switch_ternary_lines("Some(1 as UInt32)", "Empty")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_binding_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    let value: Maybe<UInt32> = flag ? Some(true) : Empty",
            "    return 1",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_binding_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    let value: Maybe<UInt32> = flag ? Some(1 as UInt32) : Empty",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_assignment_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    var value: Maybe<UInt32> = Empty",
            "    value = flag ? Some(true) : Empty",
            "    return 1",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_assignment_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo(flag: Bool) -> UInt32",
            "    var value: Maybe<UInt32> = Empty",
            "    value = flag ? Some(1 as UInt32) : Empty",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_call_argument_payload_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo(flag: Bool) -> UInt32",
            "    return consume(flag ? Some(true) : Empty)",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_ternary_call_argument_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo(flag: Bool) -> UInt32",
            "    return consume(flag ? Some(1 as UInt32) : Empty)",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_zero_payload_final_expression.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> Maybe<UInt32>",
            "    Empty",
        }
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unannotated_binding_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo() -> UInt32",
            "    let value = Some(1 as UInt32)",
            "    return consume(value)",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unannotated_zero_payload.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function demo() -> UInt32",
            "    let value = Empty",
            "    return 1",
        },
        "choice constructor 'Empty' requires an expected choice type"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_unannotated_ambiguous_name.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice LocalStatus",
            "    Ready(value: UInt32)",
            "choice RemoteStatus",
            "    Ready(value: UInt32)",
            "function demo() -> UInt32",
            "    let value = Ready(1 as UInt32)",
            "    return 1",
        },
        "choice constructor 'Ready' requires an expected choice type"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_zero_payload_call_argument.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "function consume(value: Maybe<UInt32>) -> UInt32",
            "    return 1",
            "function demo() -> UInt32",
            "    return consume(Empty)",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_element.or",
        scalar_array_constant_lines(scalar_array_constant_initializer("true")),
        "constant array initializer element type 'Bool' does not match declared element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_length.or",
        scalar_array_constant_lines(scalar_array_constant_initializer("1, 2, 3", 2)),
        "constant array initializer length 3 does not match declared length 2"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_constant_element.or",
        nested_array_constant_lines(nested_array_constant_initializer("[1, true]")),
        "constant array initializer element type 'Bool' does not match declared element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_constant_length.or",
        nested_array_constant_lines(nested_array_constant_initializer("[1, 2, 3]")),
        "constant array initializer length 3 does not match declared length 2"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_indexed_array_constant_element_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "const FLAGS: Array<Bool, 1> = [true]",
            "const FIRST_FLAG: UInt32 = FLAGS[0]",
        },
        "constant initializer type 'Bool' does not match declared constant type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_constant_field_type.or",
        header_record_constant_lines("const DEFAULT_HEADER: Header = Header([1, 2], true)"),
        "record constructor field 'version' type 'Bool' does not match expected field type 'UInt16'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_return_arity.or",
        header_record_function_lines({"    return Header([1, 2])"}),
        "record constructor 'Header' expects 2 field values but received 1"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_return_type.or",
        two_header_record_function_lines("OtherHeader", {"    return Header([1, 2], 1)"}),
        "return expression type 'Header' does not match declared type 'OtherHeader'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_assignment_type.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_record_constructor_function_argument_type.or",
        two_header_record_consumer_lines({"    return consume_header(OtherHeader([1, 2], 1))"}),
        "function argument 'header' type 'OtherHeader' does not match declared type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_final_expression_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr() -> Pointer<Byte>",
            "    \"text\"",
        },
        "pointer-returning function currently requires a structurally pointer-like expression"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_final_expression_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(base: Pointer<Byte>) -> Pointer<Byte>",
            "    raw_offset(base, 1)",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_return_ternary_rawoffset_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_word_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_binding_ternary_rawoffset_type.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_pointer_binding_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    let p: Pointer<Byte> = flag ? raw_offset(base, 1) : raw_offset(other, 1)",
            "    return p",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_assignment_ternary_rawoffset_type.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_pointer_assignment_ternary_rawoffset_success.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_pointer_call_ternary_rawoffset_type.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_pointer_call_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function consume(ptr: Pointer<Byte>) -> UInt32",
            "    return 1",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> UInt32",
            "    return consume(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_method_ternary_rawoffset_type.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_pointer_method_ternary_rawoffset_success.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_pointer_final_ternary_rawoffset_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function next_ptr(flag: Bool, base: Pointer<Byte>, other: Pointer<Byte>) -> Pointer<Byte>",
            "    flag ? raw_offset(base, 1) : raw_offset(other, 1)",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_final_if_ternary_rawoffset_type.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_pointer_ternary_pointer_source_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Buffer",
            "    data: Pointer<Byte>",
            "unsafe function first_word_ptr(flag: Bool, buf: Buffer, other: Pointer<UInt32>) -> Pointer<UInt32>",
            "    return flag ? Pointer(address_of(buf.data[0])) : raw_offset(other, 1)",
        },
        "Pointer construction source type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_address_final_expression_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function base() -> Address",
            "    \"text\"",
        },
        "address-returning function currently requires a structurally address-like expression"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_address_final_expression_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function base(buf: exclusive Buffer) -> Address",
            "    address_of(buf.data[0])",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_address_return_ternary_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "unsafe function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    return flag ? address_of(buf.data[0]) : \"text\"",
        },
        "address-returning function currently requires a structurally address-like expression"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_address_unsafe_final_ternary_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "function base(flag: Bool, buf: exclusive Buffer) -> Address",
            "    unsafe",
            "        flag ? address_of(buf.data[0]) : address_of(buf.data[1])",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_expression_type.or",
        low_level_final_read_direct_mismatch_lines("raw_read"),
        "raw_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_expression_success.or",
        low_level_final_read_direct_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_rebound_final_expression_type.or",
        low_level_final_read_rebound_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_rebound_final_expression_success.or",
        low_level_final_read_rebound_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_unsafe_block_final_expression_type.or",
        low_level_final_read_unsafe_block_direct_mismatch_lines("raw_read"),
        "raw_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_unsafe_block_final_expression_success.or",
        low_level_final_read_unsafe_block_direct_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_unsafe_block_rebound_final_expression_type.or",
        low_level_final_read_unsafe_block_rebound_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_unsafe_block_rebound_final_expression_success.or",
        low_level_final_read_unsafe_block_rebound_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_branch_final_expression_type.or",
        low_level_final_read_branch_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_branch_final_expression_success.or",
        low_level_final_read_branch_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_switch_final_expression_type.or",
        low_level_final_read_switch_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_switch_final_expression_success.or",
        low_level_final_read_switch_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_if_expression_type.or",
        low_level_final_if_read_mismatch_lines("raw_read"),
        "raw_read result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_if_expression_success.or",
        low_level_final_if_read_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_switch_expression_type.or",
        low_level_final_switch_read_mismatch_lines("raw_read"),
        "raw_read result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_final_switch_expression_success.or",
        low_level_final_switch_read_success_lines("raw_read")
    );
    assert_cli_nested_final_container_parse_cases(executable, "raw_read");
    assert_cli_final_container_return_parse_cases(executable, "raw_read");
    assert_cli_final_ternary_low_level_parse_cases(executable, "raw_read");
    assert_cli_nested_final_ternary_low_level_parse_cases(executable, "raw_read");
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_guard_final_expression_success.or",
        low_level_final_read_guard_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_guard_final_expression_type.or",
        low_level_final_read_guard_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_while_final_expression_success.or",
        low_level_final_read_while_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_repeat_final_expression_type.or",
        low_level_final_read_repeat_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_for_final_expression_success.or",
        low_level_final_read_for_success_lines("raw_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_raw_read_for_final_expression_type.or",
        low_level_final_read_for_mismatch_lines("raw_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_expression_type.or",
        low_level_final_read_direct_mismatch_lines("volatile_read"),
        "volatile_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_expression_success.or",
        low_level_final_read_direct_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_rebound_final_expression_type.or",
        low_level_final_read_rebound_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_rebound_final_expression_success.or",
        low_level_final_read_rebound_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_unsafe_block_final_expression_type.or",
        low_level_final_read_unsafe_block_direct_mismatch_lines("volatile_read"),
        "volatile_read result type 'Byte' does not match function return type 'Pointer<Byte>'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_unsafe_block_final_expression_success.or",
        low_level_final_read_unsafe_block_direct_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() /
            "orison_cli_volatile_read_unsafe_block_rebound_final_expression_type.or",
        low_level_final_read_unsafe_block_rebound_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() /
            "orison_cli_volatile_read_unsafe_block_rebound_final_expression_success.or",
        low_level_final_read_unsafe_block_rebound_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_branch_final_expression_type.or",
        low_level_final_read_branch_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_branch_final_expression_success.or",
        low_level_final_read_branch_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_switch_final_expression_type.or",
        low_level_final_read_switch_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_switch_final_expression_success.or",
        low_level_final_read_switch_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_if_expression_type.or",
        low_level_final_if_read_mismatch_lines("volatile_read"),
        "volatile_read result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_if_expression_success.or",
        low_level_final_if_read_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_switch_expression_type.or",
        low_level_final_switch_read_mismatch_lines("volatile_read"),
        "volatile_read result type 'Byte' does not match function return type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_final_switch_expression_success.or",
        low_level_final_switch_read_success_lines("volatile_read")
    );
    assert_cli_nested_final_container_parse_cases(executable, "volatile_read");
    assert_cli_final_container_return_parse_cases(executable, "volatile_read");
    assert_cli_final_ternary_low_level_parse_cases(executable, "volatile_read");
    assert_cli_nested_final_ternary_low_level_parse_cases(executable, "volatile_read");
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_guard_final_expression_success.or",
        low_level_final_read_guard_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_guard_final_expression_type.or",
        low_level_final_read_guard_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_while_final_expression_success.or",
        low_level_final_read_while_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_repeat_final_expression_type.or",
        low_level_final_read_repeat_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_for_final_expression_success.or",
        low_level_final_read_for_success_lines("volatile_read")
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_volatile_read_for_final_expression_type.or",
        low_level_final_read_for_mismatch_lines("volatile_read"),
        "final expression type 'Byte' does not match declared type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_generic_function_dependent_argument_type.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(OtherHeader([1, 2], 1), 1 as UInt16))"}
        ),
        "function argument 'pair' type 'Pair<OtherHeader, UInt16>' does not match declared type 'Pair<Header, UInt16>'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_generic_repeated_binding_argument_type.or",
        generic_same_consumer_lines({"    return same(Header([1, 2], 1), OtherHeader([1, 2], 1))"}),
        "function argument 'right' type 'OtherHeader' does not match declared type 'Header'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_generic_record_repeated_field_type.or",
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
        std::filesystem::temp_directory_path() / "orison_cli_record_choice_ternary_field_type.or",
        box_maybe_record_cli_lines("    let box: Box<UInt32> = Box(flag ? Some(true) : Empty)", false),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_choice_ternary_field_success.or",
        box_maybe_record_cli_lines("    let box: Box<UInt32> = Box(flag ? Some(1 as UInt32) : Empty)", false)
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_pointer_ternary_field_type.or",
        slot_pointer_record_cli_lines(
            "    let slot: Slot<UInt32> = Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
            false
        ),
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_record_pointer_ternary_field_success.or",
        slot_pointer_record_success_cli_lines(
            "    let slot: Slot<UInt32> = Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))",
            false
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_choice_ternary_field_type.or",
        box_maybe_record_cli_lines(
            "    let outer: Outer<UInt32> = Outer(Box(flag ? Some(true) : Empty))",
            true
        ),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_choice_ternary_field_success.or",
        box_maybe_record_cli_lines(
            "    let outer: Outer<UInt32> = Outer(Box(flag ? Some(1 as UInt32) : Empty))",
            true
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_pointer_ternary_field_type.or",
        slot_pointer_record_cli_lines(
            "    let wrapper: Wrapper<UInt32> = Wrapper(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            true
        ),
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_record_pointer_ternary_field_success.or",
        slot_pointer_record_success_cli_lines(
            "    let wrapper: Wrapper<UInt32> = Wrapper(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            true
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_payload_record_choice_ternary_field_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Item(value: Box<T>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Box(flag ? Some(true) : Empty))",
            "    return 1",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_payload_record_choice_ternary_field_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "choice Wrap<T>",
            "    Item(value: Box<T>)",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Box(flag ? Some(1 as UInt32) : Empty))",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_record_choice_ternary_field_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let boxes: Array<Box<UInt32>, 1> = [Box(flag ? Some(true) : Empty)]",
            "    return 1",
        },
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_record_choice_ternary_field_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "choice Maybe<T>",
            "    Some(value: T)",
            "    Empty",
            "record Box<T>",
            "    value: Maybe<T>",
            "function demo(flag: Bool) -> UInt32",
            "    let boxes: Array<Box<UInt32>, 1> = [Box(flag ? Some(1 as UInt32) : Empty)]",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_payload_record_pointer_ternary_field_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Item(value: Slot<T>)",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            "    return 1",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_payload_record_pointer_ternary_field_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "choice Wrap<T>",
            "    Item(value: Slot<T>)",
            "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32",
            "    let item: Wrap<UInt32> = Item(Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1)))",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_record_pointer_ternary_field_type.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "unsafe function demo(flag: Bool, base: Pointer<Byte>, other: Pointer<UInt32>) -> UInt32",
            "    let slots: Array<Slot<UInt32>, 1> = [Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]",
            "    return 1",
        },
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_record_pointer_ternary_field_success.or",
        std::vector<std::string_view> {
            "package demo.cli",
            "record Slot<T>",
            "    ptr: Pointer<T>",
            "unsafe function demo(flag: Bool, base: Pointer<UInt32>, other: Pointer<UInt32>) -> UInt32",
            "    let slots: Array<Slot<UInt32>, 1> = [Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]",
            "    return 1",
        }
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_record_choice_ternary_field_type.or",
        box_maybe_record_cli_lines(
            "    let boxes: Array<Array<Box<UInt32>, 1>, 1> = [[Box(flag ? Some(true) : Empty)]]",
            false
        ),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_record_choice_ternary_field_success.or",
        box_maybe_record_cli_lines(
            "    let boxes: Array<Array<Box<UInt32>, 1>, 1> = [[Box(flag ? Some(1 as UInt32) : Empty)]]",
            false
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_record_cli_lines(
            "    let slots: Array<Array<Slot<UInt32>, 1>, 1> = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]",
            false
        ),
        "raw_offset source pointer element type 'Byte' does not match expected pointer element type 'UInt32'"
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_record_success_cli_lines(
            "    let slots: Array<Array<Slot<UInt32>, 1>, 1> = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]",
            false
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_record_field_nested_array_record_choice_ternary_field_type.or",
        box_maybe_nested_array_field_cli_lines(
            "    let shelf: Shelf<UInt32> = Shelf([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_record_field_nested_array_record_choice_ternary_field_success.or",
        box_maybe_nested_array_field_cli_lines(
            "    let shelf: Shelf<UInt32> = Shelf([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_record_field_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_nested_array_field_cli_lines(
            "    let rack: Rack<UInt32> = Rack([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_record_field_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_nested_array_field_success_cli_lines(
            "    let rack: Rack<UInt32> = Rack([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_record_field_nested_array_choice_constructor_ternary_payload_type.or",
        maybe_nested_array_field_cli_lines(
            "    let shelf: ChoiceShelf<UInt32> = ChoiceShelf([[flag ? Some(true) : Empty]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_record_field_nested_array_choice_constructor_ternary_payload_success.or",
        maybe_nested_array_field_cli_lines(
            "    let shelf: ChoiceShelf<UInt32> = ChoiceShelf([[flag ? Some(1 as UInt32) : Empty]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_call_argument_nested_array_record_choice_ternary_field_type.or",
        box_maybe_nested_array_argument_cli_lines(
            "    return consume([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_call_argument_nested_array_record_choice_ternary_field_success.or",
        box_maybe_nested_array_argument_cli_lines(
            "    return consume([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_call_argument_nested_array_choice_constructor_ternary_payload_type.or",
        maybe_nested_array_argument_cli_lines(
            "    return consume([[flag ? Some(true) : Empty]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_call_argument_nested_array_choice_constructor_ternary_payload_success.or",
        maybe_nested_array_argument_cli_lines(
            "    return consume([[flag ? Some(1 as UInt32) : Empty]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_method_argument_nested_array_record_choice_ternary_field_type.or",
        box_maybe_nested_array_argument_cli_lines(
            "    return consumer.consume([[Box(flag ? Some(true) : Empty)]])",
            true
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_method_argument_nested_array_record_choice_ternary_field_success.or",
        box_maybe_nested_array_argument_cli_lines(
            "    return consumer.consume([[Box(flag ? Some(1 as UInt32) : Empty)]])",
            true
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_method_argument_nested_array_choice_constructor_ternary_payload_type.or",
        maybe_nested_array_argument_cli_lines(
            "    return consumer.consume([[flag ? Some(true) : Empty]])",
            true
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_method_argument_nested_array_choice_constructor_ternary_payload_success.or",
        maybe_nested_array_argument_cli_lines(
            "    return consumer.consume([[flag ? Some(1 as UInt32) : Empty]])",
            true
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_assignment_nested_array_record_choice_ternary_field_type.or",
        box_maybe_nested_array_assignment_cli_lines(
            "    values = [[Box(flag ? Some(true) : Empty)]]"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_nested_array_record_choice_ternary_field_success.or",
        box_maybe_nested_array_assignment_cli_lines(
            "    values = [[Box(flag ? Some(1 as UInt32) : Empty)]]"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_assignment_nested_array_choice_constructor_ternary_payload_type.or",
        maybe_nested_array_assignment_cli_lines(
            "    values = [[flag ? Some(true) : Empty]]"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_nested_array_choice_constructor_ternary_payload_success.or",
        maybe_nested_array_assignment_cli_lines(
            "    values = [[flag ? Some(1 as UInt32) : Empty]]"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_assignment_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_nested_array_assignment_cli_lines(
            "    slots = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_nested_array_assignment_success_cli_lines(
            "    slots = [[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]"
        )
    );
    assert_cli_choice_payload_items_choice_ternary_failure(
        executable,
        "orison_cli_choice_payload_array_record_choice_ternary_field_type.or",
        "    let item: Wrap<UInt32> = Items([Box(flag ? Some(true) : Empty)])",
        false
    );
    assert_cli_choice_payload_items_choice_ternary_success(
        executable,
        "orison_cli_choice_payload_array_record_choice_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([Box(flag ? Some(1 as UInt32) : Empty)])",
        false
    );
    assert_cli_choice_payload_items_pointer_ternary_failure(
        executable,
        "orison_cli_choice_payload_array_record_pointer_ternary_field_type.or",
        "    let item: Wrap<UInt32> = Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))])",
        false
    );
    assert_cli_choice_payload_items_pointer_ternary_success(
        executable,
        "orison_cli_choice_payload_array_record_pointer_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))])",
        false
    );
    assert_cli_choice_payload_items_choice_ternary_failure(
        executable,
        "orison_cli_choice_payload_nested_array_record_choice_ternary_field_type.or",
        "    let item: Wrap<UInt32> = Items([[Box(flag ? Some(true) : Empty)]])",
        false,
        "Array<Array<Box<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_choice_ternary_success(
        executable,
        "orison_cli_choice_payload_nested_array_record_choice_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([[Box(flag ? Some(1 as UInt32) : Empty)]])",
        false,
        "Array<Array<Box<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_pointer_ternary_failure(
        executable,
        "orison_cli_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        "    let item: Wrap<UInt32> = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])",
        false,
        "Array<Array<Slot<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_pointer_ternary_success(
        executable,
        "orison_cli_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        "    let item: Wrap<UInt32> = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])",
        false,
        "Array<Array<Slot<T>, 1>, 1>"
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_assignment_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_items_assignment_cli_lines(
            "    item = Items([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_items_assignment_cli_lines(
            "    item = Items([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_assignment_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_items_assignment_cli_lines(
            "    item = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_items_assignment_success_cli_lines(
            "    item = Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_return_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_items_return_cli_lines(
            "    return Items([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_return_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_items_return_cli_lines(
            "    return Items([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_final_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_items_return_cli_lines(
            "    Items([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_items_return_cli_lines(
            "    Items([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_return_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_items_return_cli_lines(
            "    return Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_return_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_items_return_success_cli_lines(
            "    return Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_final_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_items_return_cli_lines(
            "    Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_items_return_success_cli_lines(
            "    Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_final_if_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_items_final_if_cli_lines(
            "        Items([[Box(flag ? Some(true) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_if_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_items_final_if_cli_lines(
            "        Items([[Box(flag ? Some(1 as UInt32) : Empty)]])"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_final_if_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_items_final_if_cli_lines(
            "        Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_if_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_items_final_if_success_cli_lines(
            "        Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]])"
        )
    );
    assert_cli_choice_payload_items_choice_ternary_failure(
        executable,
        "orison_cli_record_field_choice_payload_array_record_choice_ternary_field_type.or",
        "    let holder: Holder<UInt32> = Holder(Items([Box(flag ? Some(true) : Empty)]))",
        true
    );
    assert_cli_choice_payload_items_choice_ternary_success(
        executable,
        "orison_cli_record_field_choice_payload_array_record_choice_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([Box(flag ? Some(1 as UInt32) : Empty)]))",
        true
    );
    assert_cli_choice_payload_items_pointer_ternary_failure(
        executable,
        "orison_cli_record_field_choice_payload_array_record_pointer_ternary_field_type.or",
        "    let holder: Holder<UInt32> = Holder(Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]))",
        true
    );
    assert_cli_choice_payload_items_pointer_ternary_success(
        executable,
        "orison_cli_record_field_choice_payload_array_record_pointer_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]))",
        true
    );
    assert_cli_choice_payload_items_choice_ternary_failure(
        executable,
        "orison_cli_record_field_choice_payload_nested_array_record_choice_ternary_field_type.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Box(flag ? Some(true) : Empty)]]))",
        true,
        "Array<Array<Box<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_choice_ternary_success(
        executable,
        "orison_cli_record_field_choice_payload_nested_array_record_choice_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))",
        true,
        "Array<Array<Box<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_pointer_ternary_failure(
        executable,
        "orison_cli_record_field_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))",
        true,
        "Array<Array<Slot<T>, 1>, 1>"
    );
    assert_cli_choice_payload_items_pointer_ternary_success(
        executable,
        "orison_cli_record_field_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        "    let holder: Holder<UInt32> = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))",
        true,
        "Array<Array<Slot<T>, 1>, 1>"
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_assignment_holder_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_holder_items_assignment_cli_lines(
            "    holder = Holder(Items([[Box(flag ? Some(true) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_holder_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_holder_items_assignment_cli_lines(
            "    holder = Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_assignment_holder_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_holder_items_assignment_cli_lines(
            "    holder = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_assignment_holder_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_holder_items_assignment_success_cli_lines(
            "    holder = Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_return_holder_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_holder_items_return_cli_lines(
            "    return Holder(Items([[Box(flag ? Some(true) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_return_holder_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_holder_items_return_cli_lines(
            "    return Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_final_holder_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_holder_items_return_cli_lines(
            "    Holder(Items([[Box(flag ? Some(true) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_holder_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_holder_items_return_cli_lines(
            "    Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_return_holder_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_holder_items_return_cli_lines(
            "    return Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_return_holder_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_holder_items_return_success_cli_lines(
            "    return Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_final_holder_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_holder_items_return_cli_lines(
            "    Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_holder_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_holder_items_return_success_cli_lines(
            "    Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_choice_context_failure(
        executable,
        "orison_cli_final_if_holder_choice_payload_nested_array_record_choice_ternary_field_type.or",
        box_maybe_holder_items_final_if_cli_lines(
            "        Holder(Items([[Box(flag ? Some(true) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_if_holder_choice_payload_nested_array_record_choice_ternary_field_success.or",
        box_maybe_holder_items_final_if_cli_lines(
            "        Holder(Items([[Box(flag ? Some(1 as UInt32) : Empty)]]))"
        )
    );
    assert_cli_record_field_nested_array_pointer_context_failure(
        executable,
        "orison_cli_final_if_holder_choice_payload_nested_array_record_pointer_ternary_field_type.or",
        slot_pointer_holder_items_final_if_cli_lines(
            "        Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_record_field_nested_array_context_success(
        executable,
        "orison_cli_final_if_holder_choice_payload_nested_array_record_pointer_ternary_field_success.or",
        slot_pointer_holder_items_final_if_success_cli_lines(
            "        Holder(Items([[Slot(flag ? raw_offset(base, 1) : raw_offset(other, 1))]]))"
        )
    );
    assert_cli_parse_success(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_generic_function_dependent_same_width_integer.or",
        generic_pair_consumer_lines(
            {"    return consume_pair(Header([1, 2], 1), Pair(Header([1, 2], 1), 1 as Int16))"}
        )
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_unknown_reference.or",
        scalar_array_constant_lines(scalar_array_constant_initializer("STATUS_LOW")),
        "constant initializer references unknown name 'STATUS_LOW'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_direct_cycle.or",
        scalar_array_constant_lines(scalar_array_constant_initializer("MAGIC[0]")),
        "constant initializer cycle includes 'MAGIC'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_address_array_constant_element.or",
        scalar_array_constant_lines("const UART_REGISTERS: Array<Address, 1> = [\"uart\"]"),
        "constant array initializer element type 'Text' does not match declared element type 'Address'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_pointer_array_constant_construction.or",
        low_level_array_constant_lines("const UART_POINTERS: Array<Pointer<UInt32>, 1> = [Pointer(UART0_DATA)]"),
        "constant initializer cannot use Pointer construction"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_nested_choice_array_constant_payload.or",
        boxed_maybe_array_choice_constant_lines(
            "const DEFAULT_VALUES: Array<Boxed<Maybe<UInt32>>, 1> = [Wrap(Some(true))]"
        ),
        "choice constructor payload type 'Bool' does not match expected payload type 'UInt32'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_constant_array_payload_length.or",
        maybe_choice_constant_lines("const DEFAULT_VALUE: Maybe<Array<UInt32, 2>> = Some([1, 2, 3])"),
        "constant array initializer length 3 does not match declared length 2"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_function_call.or",
        scalar_array_constant_lines(
            scalar_array_constant_initializer("mask(0xFFFF)"),
            {"function mask(value: UInt32) -> UInt32", "    return value bit_and 0xFF"}
        ),
        "constant initializer cannot call function 'mask'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_function_call.or",
        maybe_choice_constant_lines(
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([mask(0xFFFF)])",
            {"function mask(value: UInt32) -> UInt32", "    return value bit_and 0xFF"}
        ),
        "constant initializer cannot call function 'mask'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_unsafe_intrinsic.or",
        scalar_array_constant_lines(
            scalar_array_constant_initializer("raw_read(UART_STATUS)"),
            {"const UART_STATUS: Address = 0x4000_1000"}
        ),
        "constant initializer cannot use unsafe intrinsic 'raw_read'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_unsafe_intrinsic.or",
        maybe_choice_constant_lines(
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([raw_read(UART_STATUS)])",
            {"const UART_STATUS: Address = 0x4000_1000"}
        ),
        "constant initializer cannot use unsafe intrinsic 'raw_read'"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_await.or",
        scalar_array_constant_lines(
            scalar_array_constant_initializer("await STATUS_MASK"),
            {"const STATUS_MASK: UInt32 = 0xFF"}
        ),
        "constant initializer cannot use await expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_await.or",
        maybe_choice_constant_lines(
            "const DEFAULT_VALUE: Maybe<Array<UInt32, 1>> = Some([await STATUS_MASK])",
            {"const STATUS_MASK: UInt32 = 0xFF"}
        ),
        "constant initializer cannot use await expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_task.or",
        scalar_array_block_runtime_constant_lines("task"),
        "constant initializer cannot use task expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_task.or",
        maybe_array_payload_block_runtime_constant_lines("task"),
        "constant initializer cannot use task expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_array_constant_thread.or",
        scalar_array_block_runtime_constant_lines("thread"),
        "constant initializer cannot use thread expression"
    );
    assert_cli_parse_failure(
        executable,
        std::filesystem::temp_directory_path() / "orison_cli_choice_array_payload_thread.or",
        maybe_array_payload_block_runtime_constant_lines("thread"),
        "constant initializer cannot use thread expression"
    );
    std::filesystem::remove_all(smoke_temp_root);
    return 0;
}
